#include <Swoosh/Segue.h>
#include <Swoosh/ActivityController.h>
#include <Segues/PixelateBlackWashFade.h>
#include <Segues/BlackWashFade.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/VerticalOpen.h>
#include <Poco/Net/NetException.h>
#include <Poco/InflatingStream.h>
#include <Poco/MemoryStream.h>

#include <filesystem>
#include <fstream>

#include "bnOverworldOnlineArea.h"
#include "bnOverworldTileType.h"
#include "bnOverworldTileBehaviors.h"
#include "bnOverworldObjectType.h"
#include "bnOverworldPollingPacketProcessor.h"
#include "../bnGameSession.h"
#include "../bnMath.h"
#include "../bnMobPackageManager.h"
#include "../bnLuaLibraryPackageManager.h"
#include "../bnPlayerPackageManager.h"
#include "../bnBlockPackageManager.h"
#include "../bindings/bnScriptedCard.h"
#include "../bindings/bnLuaLibrary.h"
#include "../bindings/bnScriptedPlayer.h"
#include "../bindings/bnScriptedBlock.h"
#include "../bnMessageQuestion.h"
#include "../bnPlayerCustScene.h"
#include "../bnSelectNaviScene.h"
#include "../battlescene/bnMobBattleScene.h"
#include "../battlescene/bnFreedomMissionMobScene.h"
#include "../netplay/bnBufferWriter.h"
#include "../netplay/battlescene/bnNetworkBattleScene.h"
#include "../netplay/bnNetPlayConfig.h"
#include "../netplay/bnDownloadScene.h"
#include "../bindings/bnScriptedMob.h"

using namespace swoosh::types;
constexpr float ROLLING_WINDOW_SMOOTHING = 3.0f;
constexpr float SECONDS_PER_MOVEMENT = 1.f / 10.f;
constexpr long long MAX_IDLE_MS = 1000;
constexpr float MIN_IDLE_MOVEMENT = 1.f;

static long long GetSteadyTime() {
  return std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::steady_clock::now().time_since_epoch()).count();
}

Overworld::OnlineArea::OnlineArea(
  swoosh::ActivityController& controller,
  const std::string& host,
  uint16_t port,
  const std::string& connectData,
  uint16_t maxPayloadSize
) :
  host(host),
  port(port),
  SceneBase(controller),
  transitionText(Font::Style::small),
  nameText(Font::Style::small),
  connectData(connectData),
  maxPayloadSize(maxPayloadSize),
  serverAssetManager(host, port),
  identityManager(host, port)
{
  RefreshNaviSprite();

  try {
    auto remoteAddress = Poco::Net::SocketAddress(host, port);
    packetProcessor = std::make_shared<Overworld::PacketProcessor>(remoteAddress, maxPayloadSize);
    packetProcessor->SetPacketBodyCallback([this](auto& body) { processPacketBody(body); });

    Net().AddHandler(remoteAddress, packetProcessor);

    sendLoginSignal();
    sendAssetsFound();
    sendAvatarChangeSignal();
    sendRequestJoinSignal();
  }
  catch (std::exception& e) {
    Logger::Logf(LogLevel::critical, e.what());
    leave();
  }
  catch (...) {
    Logger::Logf(LogLevel::critical, "Unknown exception thrown. Aborting join.");
    leave();
  }

  transitionText.setScale(2, 2);
  transitionText.SetString("Connecting...");

  lastFrameNaviId = this->GetCurrentNaviID();

  // emotes
  auto windowSize = getController().getVirtualWindowSize();
  auto emoteWidget = std::make_shared<EmoteWidget>();
  emoteWidget->setPosition(windowSize.x / 2.f, windowSize.y / 2.f);
  emoteWidget->OnSelect(std::bind(&Overworld::OnlineArea::sendEmoteSignal, this, std::placeholders::_1));
  GetMenuSystem().BindMenu(InputEvents::pressed_option, emoteWidget);

  auto player = GetPlayer();

  emoteNode = std::make_shared<Overworld::EmoteNode>();
  RefreshEmotePosition(*emoteNode, *player);
  emoteNode->SetLayer(-100);
  emoteNode->setScale(0.5f, 0.5f);
  player->AddNode(emoteNode);

  // ensure the existence of these package partitions
  getController().BlockPackagePartitioner().CreateNamespace(Game::ServerPartition);
  getController().CardPackagePartitioner().CreateNamespace(Game::ServerPartition);
  getController().MobPackagePartitioner().CreateNamespace(Game::ServerPartition);
  getController().LuaLibraryPackagePartitioner().CreateNamespace(Game::ServerPartition);
  getController().PlayerPackagePartitioner().CreateNamespace(Game::ServerPartition);
}

Overworld::OnlineArea::~OnlineArea()
{
}

std::optional<Overworld::OnlineArea::AbstractUser> Overworld::OnlineArea::GetAbstractUser(const std::string& id)
{
  if (id == ticket) {
    return AbstractUser{
      GetPlayer(),
      nullptr,
      emoteNode,
      GetTeleportController(),
      propertyAnimator,
      true
    };
  }

  auto iter = onlinePlayers.find(id);

  if (iter != onlinePlayers.end()) {
    auto& onlinePlayer = iter->second;

    return AbstractUser{
      onlinePlayer.actor,
      onlinePlayer.marker,
      onlinePlayer.emoteNode,
      onlinePlayer.teleportController,
      onlinePlayer.propertyAnimator,
      onlinePlayer.solid
    };
  }

  return {};
}

// makes sure we transition while the scene is in focus
void Overworld::OnlineArea::AddSceneChangeTask(const std::function<void()>& task) {
  // we could check if we're in focus here, but we push some tasks that try to block changes for a frame (copyScreen)
  sceneChangeTasks.push(task);
}

void Overworld::OnlineArea::SetAvatarAsSpeaker() {
  PlayerMeta& meta = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(GetCurrentNaviID());
  std::shared_ptr<sf::Texture> mugshot = Textures().LoadFromFile(meta.GetMugshotTexturePath());
  GetMenuSystem().SetNextSpeaker(sf::Sprite(*mugshot), meta.GetMugshotAnimationPath());
}

void Overworld::OnlineArea::onUpdate(double elapsed)
{
  if (IsInFocus() && tryPopScene) {
    using effect = segue<PixelateBlackWashFade>;
    if (getController().pop<effect>()) {
      tryPopScene = false;
    }
  }

  if (!packetProcessor) {
    return;
  }

  if (packetProcessor->TimedOut()) {
    leave();
    return;
  }

  if (!isConnected) {
    return;
  }

  if (IsInFocus() && !sceneChangeTasks.empty()) {
    sceneChangeTasks.front()();
    sceneChangeTasks.pop();
  }

  updateOtherPlayers(elapsed);
  updatePlayer(elapsed);

  movementTimer.update(sf::seconds(static_cast<float>(elapsed)));

  if (movementTimer.getElapsed().asSeconds() > SECONDS_PER_MOVEMENT) {
    movementTimer.reset();
    sendPositionSignal();
  }

  // handle camera locking and player tracking
  if (trackedPlayer && serverCameraController.IsQueueEmpty()) {
    auto it = onlinePlayers.find(*trackedPlayer);

    if (it == onlinePlayers.end()) {
      trackedPlayer = {};
    }
    else {
      auto position = it->second.actor->Get3DPosition();
      serverCameraController.MoveCamera(GetMap().WorldToScreen(position));
    }
  }

  if (serverCameraController.IsLocked() || trackedPlayer) {
    warpCameraController.UnlockCamera();
  }

  if (serverCameraController.IsLocked() || warpCameraController.IsLocked() || trackedPlayer) {
    LockCamera();
  }

  SceneBase::onUpdate(elapsed);

  auto& camera = GetCamera();
  warpCameraController.UpdateCamera(float(elapsed), camera);
  serverCameraController.UpdateCamera(float(elapsed), camera);
  camera.Update(0);
  UnlockCamera(); // reset lock, we'll lock it later if we need to
}

void Overworld::OnlineArea::ResetPVPStep(bool failed)
{
  this->remoteNaviBlocks.clear();

  if (failed) {
    sendBattleResultsSignal(BattleResults{});
  }

  if (netBattleProcessor) {
    Net().DropProcessor(netBattleProcessor);
    netBattleProcessor = nullptr;
  }

  canProceedToBattle = false;
}

void Overworld::OnlineArea::RemovePackages() {
  Logger::Log(LogLevel::debug, "Removing server packages");
  getController().BlockPackagePartitioner().GetPartition(Game::ServerPartition).ClearPackages();
  getController().CardPackagePartitioner().GetPartition(Game::ServerPartition).ClearPackages();
  getController().MobPackagePartitioner().GetPartition(Game::ServerPartition).ClearPackages();
  getController().LuaLibraryPackagePartitioner().GetPartition(Game::ServerPartition).ClearPackages();
  getController().PlayerPackagePartitioner().GetPartition(Game::ServerPartition).ClearPackages();
}

void Overworld::OnlineArea::updateOtherPlayers(double elapsed) {
  // remove players before update, to prevent removed players from being added to sprite layers
  // players do not have a shared pointer to the emoteNode
  // a segfault would occur if this loop is placed after Scene::onUpdate due to emoteNode being deleted
  for (const auto& remove : removePlayers) {
    auto it = onlinePlayers.find(remove);

    if (it == onlinePlayers.end()) {
      Logger::Logf(LogLevel::info, "Removed non existent Player %s", remove.c_str());
      continue;
    }

    auto& player = it->second;
    RemoveActor(player.actor);
    RemoveSprite(player.teleportController.GetBeam());
    GetMinimap().RemovePlayerMarker(player.marker);

    onlinePlayers.erase(remove);
  }

  removePlayers.clear();

  auto currentTime = GetSteadyTime();
  auto& map = GetMap();

  // update other players
  for (auto& pair : onlinePlayers) {
    auto& onlinePlayer = pair.second;
    auto& actor = onlinePlayer.actor;

    onlinePlayer.teleportController.Update(elapsed);
    onlinePlayer.emoteNode->Update(elapsed);

    onlinePlayer.propertyAnimator.Update(*actor, elapsed);

    if (onlinePlayer.propertyAnimator.IsAnimatingPosition()) {
      continue;
    }

    // tile types such as ice can affect animation speed
    // set back to default
    actor->SetAnimationSpeed(1.0f);

    auto deltaTime = static_cast<double>(currentTime - onlinePlayer.timestamp) / 1000.0;
    auto delta = onlinePlayer.endBroadcastPos - onlinePlayer.startBroadcastPos;
    auto screenDelta = map.WorldToScreen(delta);
    float distance = Hypotenuse({ screenDelta.x, screenDelta.y });
    double expectedTime = onlinePlayer.lagWindow.GetEMA();
    float alpha = static_cast<float>(ease::linear(deltaTime, expectedTime, 1.0));

    auto newPos = RoundXY(onlinePlayer.startBroadcastPos + delta * alpha);
    actor->Set3DPosition(newPos);

    if (onlinePlayer.propertyAnimator.IsAnimating() && actor->IsPlayingCustomAnimation()) {
      // skip animating the player if they're being animated by the property animator
      continue;
    }

    Direction newHeading = Actor::MakeDirectionFromVector({ delta.x, delta.y });
    auto tile = map.GetTileFromWorld(newPos);

    if (tile) {
      auto metaPtr = map.GetTileMeta(tile->gid);

      if (metaPtr) {
        switch (metaPtr->type) {
        case TileType::conveyor:
          actor->Face(newHeading);
          continue; // continue the loop
        case TileType::ice:
          actor->SetAnimationSpeed(0.0f);
          break; // break the switch
        }
      }
    }

    auto shouldIdle = distance <= MIN_IDLE_MOVEMENT || currentTime - onlinePlayer.lastMovementTime > MAX_IDLE_MS;

    if (shouldIdle) {
      actor->Face(onlinePlayer.idleDirection);
    }
    else if (distance <= actor->GetWalkSpeed() * expectedTime) {
      actor->Walk(newHeading, false); // Don't actually move or collide, but animate
    }
    else {
      actor->Run(newHeading, false);
    }
  }

  auto& minimap = GetMinimap();

  // update minimap markers
  for (auto& pair : onlinePlayers) {
    auto& onlinePlayer = pair.second;
    auto pos = onlinePlayer.actor->Get3DPosition();

    minimap.UpdatePlayerMarker(
      *onlinePlayer.marker,
      map.WorldToScreen(pos),
      map.IsConcealed(sf::Vector2i(pos.x, pos.y), (int) pos.z)
    );
  }
}

void Overworld::OnlineArea::updatePlayer(double elapsed) {
  auto player = GetPlayer();
  auto playerPos = player->Get3DPosition();

  TileBehaviors::UpdateActor(*this, *player, propertyAnimator);

  propertyAnimator.Update(*player, elapsed);
  emoteNode->Update(elapsed);

  if (serverLockedInput || propertyAnimator.IsAnimatingPosition()) {
    LockInput();

    if (propertyAnimator.IsAnimatingPosition()) {
      // release actor to prevent animation override
      GetPlayerController().ReleaseActor();
    }
  }
  else if(!IsCameraControlsEnabled()) {
    UnlockInput();
    GetPlayerController().ControlActor(player);
  }

  // move the emote above the player's head
  RefreshEmotePosition(*emoteNode, *player);
  std::string currentNaviId = GetCurrentNaviID();
  if (lastFrameNaviId != currentNaviId) {
    sendAvatarChangeSignal();
    lastFrameNaviId = currentNaviId;
  }

  if (!IsInputLocked()) {
    auto& menuSystem = GetMenuSystem();

    if (menuSystem.IsClosed() && Input().Has(InputEvents::pressed_shoulder_right)) {
      SetAvatarAsSpeaker();

      menuSystem.EnqueueQuestion("Return to your homepage?", [this](bool result) {
        if (result) {
          GetTeleportController().TeleportOut(GetPlayer()).onFinish.Slot([this] {
            this->leave();
          });
        }
      });

      player->Face(Direction::down_right);
    }

    if (playerPos.x != lastPosition.x || playerPos.y != lastPosition.y) {
      // only need to handle this if the player has moved
      detectWarp();
    }
  }

  lastPosition = playerPos;
}

void Overworld::OnlineArea::detectWarp() {
  auto player = GetPlayer();
  auto& teleportController = GetTeleportController();

  if (!teleportController.IsComplete()) {
    return;
  }

  auto layerIndex = player->GetLayer();

  auto& map = GetMap();

  if (layerIndex < 0 || layerIndex >= warps.size()) {
    return;
  }

  auto playerPos = player->getPosition();

  for (auto* tileObjectPtr : warps[layerIndex]) {
    auto& tileObject = *tileObjectPtr;

    if (!tileObject.Intersects(map, playerPos.x, playerPos.y)) {
      continue;
    }

    auto& command = teleportController.TeleportOut(player);

    auto interpolateTime = sf::seconds(0.5f);
    auto position3 = sf::Vector3f(tileObject.position.x, tileObject.position.y, float(layerIndex));

    switch (tileObject.type) {
    case ObjectType::home_warp: {
      warpCameraController.QueueMoveCamera(map.WorldToScreen(position3), interpolateTime);

      command.onFinish.Slot([=] {
        leave();
      });
      break;
    }
    case ObjectType::server_warp: {
      warpCameraController.QueueMoveCamera(map.WorldToScreen(position3), interpolateTime);

      auto host = tileObject.customProperties.GetProperty("address");
      auto port = (uint16_t)tileObject.customProperties.GetPropertyInt("port");
      auto data = tileObject.customProperties.GetProperty("data");

      command.onFinish.Slot([=] {
        transferServer(host, port, data, true);
      });
      break;
    }
    case ObjectType::position_warp: {
      auto targetTilePos = sf::Vector2f(
        tileObject.customProperties.GetPropertyFloat("x"),
        tileObject.customProperties.GetPropertyFloat("y")
      );

      auto targetWorldPos = map.TileToWorld(targetTilePos);

      auto targetPosition = sf::Vector3f(targetWorldPos.x, targetWorldPos.y, tileObject.customProperties.GetPropertyFloat("z"));
      auto direction = FromString(tileObject.customProperties.GetProperty("direction"));

      sf::Vector2f player_pos = { player->getPosition().x, player->getPosition().y };
      float distance = std::pow(targetWorldPos.x - player_pos.x, 2.0f) + std::pow(targetWorldPos.y - player_pos.y, 2.0f);

      // this is a magic number - this is about as close to 2 warps that are 8 blocks away vertically
      // (expression is also squared)
      if (distance < 40'000) {
        warpCameraController.QueueWaneCamera(map.WorldToScreen(targetPosition), interpolateTime, 0.55f);
      }

      command.onFinish.Slot([=] {
        teleportIn(targetPosition, Orthographic(direction));
        warpCameraController.QueueUnlockCamera();
      });
      break;
    }
    default:
      warpCameraController.QueueMoveCamera(map.WorldToScreen(position3), interpolateTime);

      command.onFinish.Slot([=] {
        sendCustomWarpSignal(tileObject.id);
      });
    }

    break;
  }
}

void Overworld::OnlineArea::onDraw(IRenderer& renderer)
{
  if (!isConnected) {
    auto view = getController().getVirtualWindowSize();
    int precision = 1;

    transitionText.setPosition(view.x * 0.5f, view.y * 0.5f);
    transitionText.setOrigin(transitionText.GetLocalBounds().width * 0.5f, transitionText.GetLocalBounds().height * 0.5f);
    renderer.submit(&transitionText);
    return;
  }

  SceneBase::onDraw(renderer);

  // Copy the contents if requested this frame
  // TODO: there needs be a mechanism to copy contents of the screen if a scene requests one...
  //    NOTE that this no longer works because of the new pipeline...
  if (copyScreen) {
    renderer.display();
    screen = renderer.getTexture();
    copyScreen = false;
  }

  if (GetMenuSystem().IsFullscreen()) {
    return;
  }

  auto& window = getController().getWindow();
  auto mousei = sf::Mouse::getPosition(window);
  auto mousef = window.mapPixelToCoords(mousei);

  // NOTE: Uncomment below for debug mouse cursor
  /*
  sf::RectangleShape rect({ 2.f, 2.f });
  rect.setFillColor(sf::Color::Red);
  rect.setPosition(mousef);
  surface.draw(rect);
  */

  auto& map = GetMap();

  auto topLayer = -1;
  auto topY = std::numeric_limits<float>::min();
  std::string topName;

  auto testActor = [&](Actor& actor) {
    auto& name = actor.GetName();
    auto layer = (int)std::ceil(actor.GetElevation());
    auto screenY = map.WorldToScreen(actor.Get3DPosition()).y;

    if (name == "" || layer < topLayer || screenY <= topY) {
      return;
    }

    if (IsMouseHovering(actor)) {
      topLayer = layer;
      topName = name;
      topY = screenY;
    }
  };

  for (auto& pair : onlinePlayers) {
    auto id = pair.first;

    if (excludedActors.find(id) != excludedActors.end()) {
      // actor is excluded, do not display on hover
      continue;
    }

    auto& onlinePlayer = pair.second;
    auto& actor = *onlinePlayer.actor;

    testActor(actor);
  }

  testActor(*GetPlayer());

  mousef.x += 10.f;

  nameText.setPosition(mousef);
  nameText.SetString(topName);
  nameText.setOrigin(-5.0f, -2.f);

  if (topName.size()) {
    sf::FloatRect nameBounds = nameText.GetWorldBounds();
    sf::RectangleShape rect({ nameBounds.width + 10.f, nameBounds.height + 4.f });
    rect.setFillColor(sf::Color(0, 0, 0, 100));
    rect.setPosition(mousef);
    renderer.submit(Clone(rect));
    renderer.submit(&nameText);
  }
}

void Overworld::OnlineArea::onStart()
{
  SceneBase::onStart();
  movementTimer.start();
}

void Overworld::OnlineArea::onEnd()
{
  if (!cleanedUp) {
    Logger::Log(LogLevel::critical, "OverworldOnlineArea::cleanup() call missed, may have unintended effects from transition period");
    cleanup();
  }
}

void Overworld::OnlineArea::onLeave()
{
  SceneBase::onLeave();

  if (packetProcessor) {
    packetProcessor->SetBackground();
  }
}

void Overworld::OnlineArea::onResume()
{
  SceneBase::onResume();

  if (packetProcessor) {
    packetProcessor->SetForeground();
  }

  switch (returningFrom) {
  case ReturningScene::DownloadScene:
    if (!canProceedToBattle) {
      // download scene failed, give up on pvp
      ResetPVPStep(true);
    }
    break;
  case ReturningScene::BattleScene:
    ResetPVPStep();
    break;
  case ReturningScene::VendorScene:
    sendShopCloseSignal();
    break;
  }

  returningFrom = ReturningScene::Null;
}

void Overworld::OnlineArea::OnTileCollision() { }

void Overworld::OnlineArea::OnInteract(Interaction type) {
  auto& map = GetMap();
  auto tileSize = map.GetTileSize();

  auto playerActor = GetPlayer();
  auto frontPosition = playerActor->PositionInFrontOf();

  // check to see what tile we pressed talk to
  auto layerIndex = playerActor->GetLayer();

  if (layerIndex >= 0 && layerIndex < map.GetLayerCount()) {
    auto& layer = map.GetLayer(layerIndex);

    for (auto& tileObject : layer.GetTileObjects()) {
      auto interactable = tileObject.visible || tileObject.solid;

      if (interactable && tileObject.Intersects(map, frontPosition.x, frontPosition.y)) {
        sendObjectInteractionSignal(tileObject.id, type);

        // block other interactions with return
        return;
      }
    }
  }

  auto positionInFrontOffset = frontPosition - playerActor->getPosition();
  auto elevation = playerActor->GetElevation();

  for (const auto& other : GetSpatialMap().GetChunk(frontPosition.x, frontPosition.y)) {
    if (playerActor == other) continue;

    auto elevationDifference = std::fabs(other->GetElevation() - elevation);

    if (elevationDifference >= 1.0f) continue;

    auto collision = playerActor->CollidesWith(*other, positionInFrontOffset);

    if (collision) {
      other->Interact(playerActor, type);

      // block other interactions with return
      return;
    }
  }

  sendTileInteractionSignal(
    frontPosition.x / (float)(tileSize.x / 2),
    frontPosition.y / tileSize.y,
    playerActor->GetElevation(),
    type
  );
}

bool Overworld::OnlineArea::positionIsInWarp(sf::Vector3f position) {
  auto& map = GetMap();
  auto layerCount = map.GetLayerCount();

  if (position.z < 0 || position.z >= layerCount) return false;

  auto& warpLayer = warps[(size_t)position.z];

  for (auto object : warpLayer) {
    if (object->Intersects(map, position.x, position.y)) {
      return true;
    }
  }

  return false;
}

Overworld::TeleportController::Command& Overworld::OnlineArea::teleportIn(sf::Vector3f position, Direction direction)
{
  auto actor = GetPlayer();

  if (!positionIsInWarp(position)) {
    actor->Face(direction);
    direction = Direction::none;
  }

  return GetTeleportController().TeleportIn(actor, position, direction);
}

void Overworld::OnlineArea::transferServer(const std::string& host, uint16_t port, std::string data, bool warpOut) {
  auto reportFailure = [=] {
    SetAvatarAsSpeaker();
    GetMenuSystem().EnqueueMessage("Looks like the next area is offline...");
  };

  auto handleFail = [=] {
    if (warpOut) {
      auto player = GetPlayer();
      auto position = player->Get3DPosition();
      auto direction = Reverse(player->GetHeading());
      auto& command = GetTeleportController().TeleportIn(player, position, direction);
      warpCameraController.UnlockCamera();

      command.onFinish.Slot(reportFailure);
    }
    else {
      reportFailure();
    }

    transferringServers = false;
  };

  auto attemptTransfer = [=] {
    Poco::Net::SocketAddress remoteAddress;

    try {
      remoteAddress = Poco::Net::SocketAddress(host, port);
    }
    catch (Poco::IOException&) {
      handleFail();
      return;
    }

    auto packetProcessor = std::make_shared<Overworld::PollingPacketProcessor>(
      remoteAddress,
      Net().GetMaxPayloadSize()
      );

    packetProcessor->SetStatusHandler([this, host, port, data, handleFail, packetProcessor = packetProcessor.get()](auto status, auto maxPayloadSize) {
      if (status == ServerStatus::online) {
        cleanup();
        getController().replace<segue<BlackWashFade>::to<Overworld::OnlineArea>>(host, port, data, maxPayloadSize);
      }
      else {
        handleFail();
      }

      Net().DropProcessor(packetProcessor);
    });

    Net().AddHandler(remoteAddress, packetProcessor);
  };

  if (warpOut) {
    auto& command = GetTeleportController().TeleportOut(GetPlayer());
    command.onFinish.Slot(attemptTransfer);
  }
  else {
    attemptTransfer();
  }

  transferringServers = true;
}

void Overworld::OnlineArea::processPacketBody(const Poco::Buffer<char>& data)
{
  if (!packetProcessor) {
    // an exception in a synchronizedPackets loop can lead us here
    return;
  }

  BufferReader reader;

  try {
    auto sig = reader.Read<ServerEvents>(data);

    if (synchronizingUpdates && sig != ServerEvents::end_synchronization) {
      synchronizedPackets.push_back(data);
      return;
    }

    switch (sig) {
    case ServerEvents::authorize:
      receiveAuthorizeSignal(reader, data);
      break;
    case ServerEvents::login:
      receiveLoginSignal(reader, data);
      break;
    case ServerEvents::connection_complete:
      isConnected = true;
      sendReadySignal();
      break;
    case ServerEvents::transfer_warp:
      receiveTransferWarpSignal(reader, data);
      break;
    case ServerEvents::transfer_start:
      receiveTransferStartSignal(reader, data);
      break;
    case ServerEvents::transfer_complete:
      receiveTransferCompleteSignal(reader, data);
      break;
    case ServerEvents::transfer_server:
      receiveTransferServerSignal(reader, data);
      break;
    case ServerEvents::kick:
      if (!transferringServers) {
        // ignore kick signal if we're leaving anyway
        receiveKickSignal(reader, data);
      }
      break;
    case ServerEvents::remove_asset:
      receiveAssetRemoveSignal(reader, data);
      break;
    case ServerEvents::asset_stream_start:
      receiveAssetStreamStartSignal(reader, data);
      break;
    case ServerEvents::asset_stream:
      receiveAssetStreamSignal(reader, data);
      break;
    case ServerEvents::preload:
      receivePreloadSignal(reader, data);
      break;
    case ServerEvents::custom_emotes_path:
      receiveCustomEmotesPathSignal(reader, data);
      break;
    case ServerEvents::map:
      receiveMapSignal(reader, data);
      break;
    case ServerEvents::health:
      receiveHealthSignal(reader, data);
      break;
    case ServerEvents::emotion:
      receiveEmotionSignal(reader, data);
      break;
    case ServerEvents::money:
      receiveMoneySignal(reader, data);
      break;
    case ServerEvents::add_item:
      receiveItemSignal(reader, data);
      break;
    case ServerEvents::remove_item:
      receiveRemoveItemSignal(reader, data);
      break;
    case ServerEvents::play_sound:
      receivePlaySoundSignal(reader, data);
      break;
    case ServerEvents::exclude_object:
      receiveExcludeObjectSignal(reader, data);
      break;
    case ServerEvents::include_object:
      receiveIncludeObjectSignal(reader, data);
      break;
    case ServerEvents::exclude_actor:
      receiveExcludeActorSignal(reader, data);
      break;
    case ServerEvents::include_actor:
      receiveIncludeActorSignal(reader, data);
      break;
    case ServerEvents::move_camera:
      receiveMoveCameraSignal(reader, data);
      break;
    case ServerEvents::slide_camera:
      receiveSlideCameraSignal(reader, data);
      break;
    case ServerEvents::shake_camera:
      receiveShakeCameraSignal(reader, data);
      break;
    case ServerEvents::fade_camera:
      receiveFadeCameraSignal(reader, data);
      break;
    case ServerEvents::track_with_camera:
      receiveTrackWithCameraSignal(reader, data);
      break;
    case ServerEvents::enable_camera_controls:
      receiveEnableCameraControls(reader, data);
      break;
    case ServerEvents::enable_camera_zoom:
      ToggleCameraZoom(true);
      break;
    case ServerEvents::disable_camera_zoom:
      ToggleCameraZoom(false);
      break;
    case ServerEvents::unlock_camera:
      serverCameraController.QueueUnlockCamera();
      break;
    case ServerEvents::lock_input:
      serverLockedInput = true;
      break;
    case ServerEvents::unlock_input:
      serverLockedInput = false;
      break;
    case ServerEvents::teleport:
      receiveTeleportSignal(reader, data);
      break;
    case ServerEvents::message:
      receiveMessageSignal(reader, data);
      break;
    case ServerEvents::question:
      receiveQuestionSignal(reader, data);
      break;
    case ServerEvents::quiz:
      receiveQuizSignal(reader, data);
      break;
    case ServerEvents::prompt:
      receivePromptSignal(reader, data);
      break;
    case ServerEvents::open_board:
      receiveOpenBoardSignal(reader, data);
      break;
    case ServerEvents::prepend_posts:
      receivePrependPostsSignal(reader, data);
      break;
    case ServerEvents::append_posts:
      receiveAppendPostsSignal(reader, data);
      break;
    case ServerEvents::remove_post:
      receiveRemovePostSignal(reader, data);
      break;
    case ServerEvents::post_selection_ack:
      GetMenuSystem().AcknowledgeBBSSelection();
      break;
    case ServerEvents::close_bbs:
      receiveCloseBBSSignal(reader, data);
      break;
    case ServerEvents::shop_inventory:
      receiveShopInventorySignal(reader, data);
      break;
    case ServerEvents::open_shop:
      receiveOpenShopSignal(reader, data);
      break;
    case ServerEvents::load_package:
      receiveLoadPackageSignal(reader, data);
      break;
    case ServerEvents::package_offer:
      receivePackageOfferSignal(reader, data);
      break;
    case ServerEvents::mod_whitelist:
      receiveModWhitelistSignal(reader, data);
      break;
    case ServerEvents::mod_blacklist:
      receiveModBlacklistSignal(reader, data);
      break;
    case ServerEvents::initiate_mob:
      receiveMobSignal(reader, data);
      break;
    case ServerEvents::initiate_pvp:
      receivePVPSignal(reader, data);
      break;
    case ServerEvents::actor_connected:
      receiveActorConnectedSignal(reader, data);
      break;
    case ServerEvents::actor_disconnect:
      receiveActorDisconnectedSignal(reader, data);
      break;
    case ServerEvents::actor_set_name:
      receiveActorSetNameSignal(reader, data);
      break;
    case ServerEvents::actor_move_to:
      receiveActorMoveSignal(reader, data);
      break;
    case ServerEvents::actor_set_avatar:
      receiveActorSetAvatarSignal(reader, data);
      break;
    case ServerEvents::actor_emote:
      receiveActorEmoteSignal(reader, data);
      break;
    case ServerEvents::actor_animate:
      receiveActorAnimateSignal(reader, data);
      break;
    case ServerEvents::actor_keyframes:
      receiveActorKeyFramesSignal(reader, data);
      break;
    case ServerEvents::actor_minimap_color:
      receiveActorMinimapColorSignal(reader, data);
      break;
    case ServerEvents::synchronize_updates:
      synchronizingUpdates = true;
      break;
    case ServerEvents::end_synchronization:
      synchronizingUpdates = false;

      for (auto& data : synchronizedPackets) {
        processPacketBody(data);
      }

      synchronizedPackets.clear();
      break;
    }
  }
  catch (Poco::IOException& e) {
    Logger::Logf(LogLevel::critical, "OnlineArea Network exception: %s", e.displayText().c_str());

    leave();
  }
}

void Overworld::OnlineArea::CheckPlayerAgainstWhitelist()
{
  // Check if the current navi is compatible
  PlayerPackagePartitioner& partitioner = getController().PlayerPackagePartitioner();
  PlayerPackageManager& packages = partitioner.GetPartition(Game::LocalPartition);
  std::string& id = GetCurrentNaviID();
  PackageAddress addr = { Game::LocalPartition, id };

  if (!partitioner.HasPackage(addr)) return;

  const std::string& md5 = partitioner.FindPackageByAddress(addr).GetPackageFingerprint();

  GameSession& session = getController().Session();
  if (session.IsPackageAllowed({ id, md5 })) return;

  // Otherwise check to see if the player has any compatible mods
  bool anyCompatible = false;

  std::string next_id = packages.GetPackageAfter(id);

  while (next_id != id) {
    std::string next_md5 = packages.FindPackageByID(next_id).GetPackageFingerprint();
    if (session.IsPackageAllowed({ next_id, next_md5 })) {
      anyCompatible = true;
      break;
    }

    next_id = packages.GetPackageAfter(next_id);
  }

  if (anyCompatible) {
    // If so, take player to navi select screen with a message
    getController().push<segue<BlackWashFade>::to<SelectNaviScene>>(id);
    return;
  }

  // Else, inform the player they will be kicked
  auto onComplete = [this] {
    auto& command = GetTeleportController().TeleportOut(GetPlayer());
    leave();
  };

  SetAvatarAsSpeaker();
  std::string message = "This server detects none of your installed navis are allowed.\nReturning to your homepage.";
  this->GetMenuSystem().EnqueueMessage(message, onComplete);
}

void Overworld::OnlineArea::sendAssetFoundSignal(const std::string& path, uint64_t lastModified) {
  ClientEvents event{ ClientEvents::asset_found };

  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::asset_found);
  writer.WriteString<uint16_t>(buffer, path);
  writer.Write(buffer, lastModified);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendAssetsFound() {
  for (auto& [name, meta] : serverAssetManager.GetCachedAssetList()) {
    sendAssetFoundSignal(name, meta.lastModified);
  }
}

void Overworld::OnlineArea::sendAssetStreamSignal(ClientAssetType assetType, uint16_t headerSize, const char* data, size_t size) {
  size_t remainingBytes = size;

  // - 1 for asset type, - 2 for size
  const uint16_t availableRoom = maxPayloadSize - headerSize - 3;

  while (remainingBytes > 0) {
    uint16_t size = remainingBytes < availableRoom ? (uint16_t)remainingBytes : availableRoom;
    remainingBytes -= size;

    BufferWriter writer;
    Poco::Buffer<char> buffer{ 0 };
    writer.Write(buffer, ClientEvents::asset_stream);
    writer.Write(buffer, assetType);
    writer.Write(buffer, size);
    writer.WriteBytes(buffer, data, size);
    packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);

    data += size;
  }
}

void Overworld::OnlineArea::sendLoginSignal()
{
  std::string username = getController().Session().GetNick();

  if (username.empty()) {
    username = "Anon";
  }

  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::login);
  writer.WriteString<uint8_t>(buffer, username);
  writer.WriteString<uint8_t>(buffer, identityManager.GetIdentity());
  writer.WriteString<uint16_t>(buffer, connectData);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendLogoutSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::logout);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendRequestJoinSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::request_join);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendReadySignal()
{
  uint64_t currentTime = GetSteadyTime();

  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::ready);
  writer.Write(buffer, currentTime);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendTransferredOutSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::transferred_out);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendCustomWarpSignal(unsigned int tileObjectId)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::custom_warp);
  writer.Write(buffer, tileObjectId);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void Overworld::OnlineArea::sendPositionSignal()
{
  uint64_t creationTime = GetSteadyTime();

  auto& map = GetMap();
  auto tileSize = sf::Vector2f(map.GetTileSize());

  auto player = GetPlayer();
  sf::Vector2f vec = player->getPosition();
  float x = vec.x / tileSize.x * 2.0f;
  float y = vec.y / tileSize.y;
  float z = player->GetElevation();
  auto direction = Isometric(player->GetHeading());

  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::position);
  writer.Write(buffer, creationTime);
  writer.Write(buffer, x);
  writer.Write(buffer, y);
  writer.Write(buffer, z);
  writer.Write(buffer, direction);
  packetProcessor->SendPacket(Reliability::UnreliableSequenced, buffer);
}

void Overworld::OnlineArea::sendAvatarChangeSignal()
{
  sendAvatarAssetStream();

  auto& naviMeta = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(GetCurrentNaviID());
  auto naviName = naviMeta.GetName();
  auto maxHP = naviMeta.GetHP();
  auto element = GetStrFromElement(naviMeta.GetElement());

  // mark completion
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::avatar_change);
  writer.WriteString<uint8_t>(buffer, naviName);
  writer.WriteString<uint8_t>(buffer, element);
  writer.Write(buffer, maxHP);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

static std::vector<char> readBytes(std::filesystem::path texturePath) {
  size_t textureLength;
  std::vector<char> textureData;

  try {
    textureLength = std::filesystem::file_size(texturePath);
  }
  catch (std::filesystem::filesystem_error& e) {
    Logger::Logf(LogLevel::critical, "Failed to read texture \"%s\": %s", texturePath.u8string().c_str(), e.what());
    return textureData;
  }

  try {
    std::ifstream fin(texturePath, std::ios::binary);

    // prevents newlines from being skipped
    fin.unsetf(std::ios::skipws);

    textureData.reserve(textureLength);
    textureData.insert(textureData.begin(), std::istream_iterator<char>(fin), std::istream_iterator<char>());
  }
  catch (std::ifstream::failure& e) {
    Logger::Logf(LogLevel::critical, "Failed to read texture \"%s\": %s", texturePath.u8string().c_str(), e.what());
  }

  return textureData;
}

void Overworld::OnlineArea::sendAvatarAssetStream() {
  // + reliability type + id + packet type
  auto packetHeaderSize = 1 + 8 + 2;

  auto& naviMeta = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(GetCurrentNaviID());

  auto textureData = readBytes(naviMeta.GetOverworldTexturePath());
  sendAssetStreamSignal(ClientAssetType::texture, packetHeaderSize, textureData.data(), textureData.size());

  std::string animationData = FileUtil::Read(naviMeta.GetOverworldAnimationPath());
  sendAssetStreamSignal(ClientAssetType::animation, packetHeaderSize, animationData.c_str(), animationData.length());

  auto mugshotTextureData = readBytes(naviMeta.GetMugshotTexturePath());
  sendAssetStreamSignal(ClientAssetType::mugshot_texture, packetHeaderSize, mugshotTextureData.data(), mugshotTextureData.size());

  std::string mugshotAnimationData = FileUtil::Read(naviMeta.GetMugshotAnimationPath());
  sendAssetStreamSignal(ClientAssetType::mugshot_animation, packetHeaderSize, mugshotAnimationData.c_str(), mugshotAnimationData.length());
}

void Overworld::OnlineArea::sendEmoteSignal(const Overworld::Emotes emote)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::emote);
  writer.Write(buffer, emote);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void Overworld::OnlineArea::sendObjectInteractionSignal(unsigned int tileObjectId, Interaction type)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::object_interaction);
  writer.Write(buffer, tileObjectId);
  writer.Write(buffer, type);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void Overworld::OnlineArea::sendNaviInteractionSignal(const std::string& ticket, Interaction type)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::actor_interaction);
  writer.WriteString<uint16_t>(buffer, ticket);
  writer.Write(buffer, type);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void Overworld::OnlineArea::sendTileInteractionSignal(float x, float y, float z, Interaction type)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::tile_interaction);
  writer.Write(buffer, x);
  writer.Write(buffer, y);
  writer.Write(buffer, z);
  writer.Write(buffer, type);
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void Overworld::OnlineArea::sendTextBoxResponseSignal(char response)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::textbox_response);
  writer.Write(buffer, response);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendPromptResponseSignal(const std::string& response)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::prompt_response);
  writer.WriteString<uint16_t>(buffer, response);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendBoardOpenSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::board_open);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendBoardCloseSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::board_close);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendPostRequestSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::post_request);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendPostSelectSignal(const std::string& postId)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::post_selection);
  writer.WriteString<uint16_t>(buffer, postId);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendShopCloseSignal()
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::shop_close);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendShopPurchaseSignal(const std::string& itemName)
{
  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::shop_purchase);
  writer.WriteString<uint8_t>(buffer, itemName);
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::sendBattleResultsSignal(const BattleResults& battleResults) {
  if (!packetProcessor) return;

  auto time = battleResults.battleLength.asSeconds();

  BufferWriter writer;
  Poco::Buffer<char> buffer{ 0 };
  writer.Write(buffer, ClientEvents::battle_results);
  writer.Write(buffer, (int)battleResults.playerHealth);
  writer.Write(buffer, (int)battleResults.score);
  writer.Write(buffer, time);
  writer.Write(buffer, battleResults.runaway);
  writer.Write(buffer, battleResults.finalEmotion);
  writer.Write(buffer, (int)battleResults.turns);

  // npc data
  writer.Write(buffer, (uint16_t)battleResults.mobStatus.size());

  for (auto& data : battleResults.mobStatus) {
    writer.WriteString<uint8_t>(buffer, data.id);
    writer.Write(buffer, (int)data.health);
  }

  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void Overworld::OnlineArea::receiveAuthorizeSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto authAddress = reader.ReadString<uint16_t>(buffer);
  auto port = reader.Read<uint16_t>(buffer);
  auto* data = buffer.begin() + reader.GetOffset();
  auto size = buffer.size() - reader.GetOffset();

  // processor for authenticating with server
  std::shared_ptr<Overworld::PacketProcessor> authPacketProcessor;

  auto lookupString = authAddress + ":" + std::to_string(port);
  auto processorIter = authorizationProcessors.find(lookupString);

  if (processorIter == authorizationProcessors.end()) {
    // create processor
    try {
      auto remoteAddress = Poco::Net::SocketAddress(authAddress, port);
      authPacketProcessor = std::make_shared<Overworld::PacketProcessor>(remoteAddress, maxPayloadSize);

      // setup callbacks
      authPacketProcessor->SetUpdateCallback([this, lookupString, authPacketProcessor = authPacketProcessor.get()](double elapsed) {
        if (authPacketProcessor->TimedOut()) {
          Net().DropProcessor(authPacketProcessor);
          authorizationProcessors.erase(lookupString);
        }
      });

      Net().AddHandler(remoteAddress, authPacketProcessor);
      authorizationProcessors.emplace(lookupString, authPacketProcessor);
    }
    catch (std::exception& e) {
      Logger::Logf(LogLevel::critical, "Failed to authorize with %s:%d: %s.", authAddress.c_str(), port, e.what());
      return;
    }
    catch (...) {
      Logger::Logf(LogLevel::critical, "Failed to authorize with %s:%d: Unknown exception thrown.", authAddress.c_str(), port);
      return;
    }
  } else {
    // reuse existing processor
    authPacketProcessor = processorIter->second;
  }

  // create + send message
  auto externIdentity = IdentityManager(authAddress, port).GetIdentity();

  BufferWriter writer;
  Poco::Buffer<char> outBuffer{ 0 };
  writer.Write(outBuffer, ClientEvents::authorize);
  writer.WriteString<uint16_t>(outBuffer, this->host);
  writer.Write<uint16_t>(outBuffer, this->port);
  writer.WriteString<uint8_t>(outBuffer, externIdentity);
  writer.WriteBytes(outBuffer, data, size);
  authPacketProcessor->SendPacket(Reliability::Reliable, outBuffer);
}

void Overworld::OnlineArea::receiveLoginSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& map = GetMap();
  auto tileSize = map.GetTileSize();

  this->ticket = reader.ReadString<uint16_t>(buffer);
  auto warpIn = reader.Read<bool>(buffer);
  auto x = reader.Read<float>(buffer) * tileSize.x / 2.0f;
  auto y = reader.Read<float>(buffer) * tileSize.y;
  auto z = reader.Read<float>(buffer);
  auto direction = reader.Read<Direction>(buffer);

  auto spawnPos = sf::Vector3f(x, y, z);

  auto player = GetPlayer();

  if (warpIn) {
    auto& command = teleportIn(spawnPos, Orthographic(direction));
    command.onFinish.Slot([=] {
      GetPlayerController().ControlActor(player);
    });
  }
  else {
    player->Set3DPosition(spawnPos);
    player->Face(Orthographic(direction));
    GetPlayerController().ControlActor(player);
  }
}

void Overworld::OnlineArea::receiveTransferWarpSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& command = GetTeleportController().TeleportOut(GetPlayer());

  command.onFinish.Slot([this] {
    sendTransferredOutSignal();
  });
}

void Overworld::OnlineArea::receiveTransferStartSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  isConnected = false;
  transitionText.SetString("");
  excludedObjects.clear();
  removePlayers.clear();

  for (auto& [key, _] : onlinePlayers) {
    removePlayers.push_back(key);
  }
}

void Overworld::OnlineArea::receiveTransferCompleteSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  bool warpIn = reader.Read<bool>(buffer);
  auto direction = reader.Read<Direction>(buffer);
  auto worldDirection = Orthographic(direction);

  auto player = GetPlayer();
  player->Face(worldDirection);

  warpCameraController.UnlockCamera();

  if (warpIn) {
    teleportIn(player->Get3DPosition(), worldDirection);
  }

  isConnected = true;
  sendReadySignal();
}

void Overworld::OnlineArea::receiveTransferServerSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto host = reader.ReadString<uint16_t>(buffer);
  auto port = reader.Read<uint16_t>(buffer);
  auto data = reader.ReadString<uint16_t>(buffer);
  auto warpOut = reader.Read<bool>(buffer);

  transferServer(host, port, data, warpOut);
}

void Overworld::OnlineArea::receiveKickSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string kickReason = reader.ReadString<uint16_t>(buffer);
  std::string kickText = "kicked for";

  // insert padding to center the text
  auto lengthDifference = (int)kickReason.length() - (int)kickText.length();

  if (lengthDifference > 0) {
    kickText.insert(kickText.begin(), lengthDifference / 2, ' ');
  }
  else {
    kickReason.insert(kickReason.begin(), -lengthDifference / 2, ' ');
  }

  transitionText.SetString(kickText + "\n\n" + kickReason);
  isConnected = false;

  // we'll leave in update from a timeout
}

void Overworld::OnlineArea::receiveAssetRemoveSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto path = reader.ReadString<uint16_t>(buffer);

  serverAssetManager.RemoveAsset(path);
}

void Overworld::OnlineArea::receiveAssetStreamStartSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto name = reader.ReadString<uint16_t>(buffer);
  auto lastModified = reader.Read<uint64_t>(buffer);
  auto cachable = reader.Read<bool>(buffer);
  auto type = reader.Read<AssetType>(buffer);
  auto size = (size_t)reader.Read<uint64_t>(buffer);

  auto slashIndex = name.rfind("/");
  std::string shortName;

  if (slashIndex != std::string::npos) {
    shortName = name.substr(slashIndex + 1);
  }
  else {
    shortName = name;
  }

  if (shortName.length() > 20) {
    shortName = shortName.substr(0, 17) + "...";
  }

  incomingAsset = {
    name,
    shortName,
    lastModified,
    cachable,
    type,
    size,
  };

  transitionText.SetString("Downloading " + shortName + ": 0%");
}

void Overworld::OnlineArea::receiveAssetStreamSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto size = reader.Read<uint16_t>(buffer);

  incomingAsset.buffer.append(buffer.begin() + reader.GetOffset() + 2, size);

  auto progress = (float)incomingAsset.buffer.size() / (float)incomingAsset.size * 100;

  std::stringstream transitionTextStream;
  transitionTextStream << "Downloading " << incomingAsset.shortName << ": ";
  transitionTextStream << std::floor(progress);
  transitionTextStream << '%';
  transitionText.SetString(transitionTextStream.str());

  if (incomingAsset.buffer.size() < incomingAsset.size) return;

  const std::string& name = incomingAsset.name;
  auto lastModified = incomingAsset.lastModified;
  auto cachable = incomingAsset.cachable;

  BufferReader assetReader;

  switch (incomingAsset.type) {
  case AssetType::text:
    serverAssetManager.SetText(name, lastModified, assetReader.ReadString(incomingAsset.buffer, incomingAsset.buffer.size()), cachable);
    break;
  case AssetType::compressed_text: {
    auto memoryStream = Poco::MemoryInputStream(incomingAsset.buffer.begin(), incomingAsset.buffer.size());
    auto inflateStream = Poco::InflatingInputStream(memoryStream);
    std::string decompressedText(std::istreambuf_iterator(inflateStream), {});

    serverAssetManager.SetText(name, lastModified, decompressedText, cachable);
    break;
  }
  case AssetType::texture:
    serverAssetManager.SetTexture(name, lastModified, incomingAsset.buffer.begin(), incomingAsset.size, cachable);
    break;
  case AssetType::audio:
    serverAssetManager.SetAudio(name, lastModified, incomingAsset.buffer.begin(), incomingAsset.size, cachable);
    break;
  case AssetType::data:
    serverAssetManager.SetData(name, lastModified, incomingAsset.buffer.begin(), incomingAsset.size, cachable);
    break;
  }

  incomingAsset.buffer.setCapacity(0);
}

void Overworld::OnlineArea::receivePreloadSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto name = reader.ReadString<uint16_t>(buffer);

  serverAssetManager.Preload(name);
}

void Overworld::OnlineArea::receiveCustomEmotesPathSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto path = reader.ReadString<uint16_t>(buffer);

  customEmotesTexture = serverAssetManager.GetTexture(path);
  emoteNode->LoadCustomEmotes(customEmotesTexture);

  for (auto& pair : onlinePlayers) {
    auto& otherEmoteNode = pair.second.emoteNode;
    otherEmoteNode->LoadCustomEmotes(customEmotesTexture);
  }
}

void Overworld::OnlineArea::receiveMapSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto path = reader.ReadString<uint16_t>(buffer);
  auto mapBuffer = GetText(path);

  LoadMap(mapBuffer);

  auto& map = GetMap();
  auto layerCount = map.GetLayerCount();

  for (auto& [objectId, excludedData] : excludedObjects) {
    for (auto i = 0; i < layerCount; i++) {
      auto& layer = map.GetLayer(i);

      auto optional_object_ref = layer.GetTileObject(objectId);

      if (optional_object_ref) {
        auto object_ref = *optional_object_ref;
        auto& object = object_ref.get();

        excludedData.visible = object.visible;
        excludedData.solid = object.solid;

        object.visible = false;
        object.solid = false;
        break;
      }
    }
  }

  // storing special tile objects in minimap and other variables
  Minimap& minimap = GetMinimap();
  warps.resize(layerCount);

  sf::Vector2i tileSize = map.GetTileSize();

  for (size_t i = 0; i < layerCount; i++) {
    auto& warpLayer = warps[i];
    warpLayer.clear();

    for (TileObject& tileObject : map.GetLayer(i).GetTileObjects()) {
      if (ObjectType::IsWarp(tileObject.type)) {
        tileObject.solid = false;
        warpLayer.push_back(&tileObject);
      }
    }
  }
}

void Overworld::OnlineArea::receiveHealthSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  int health = reader.Read<int>(buffer);
  int maxHealth = reader.Read<int>(buffer);

  GetPlayerSession()->health = health;
  GetPlayerSession()->maxHealth = maxHealth;
}

void Overworld::OnlineArea::receiveEmotionSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  Emotion emotion = reader.Read<Emotion>(buffer);

  if (emotion >= Emotion::COUNT) {
    emotion = Emotion::normal;
  }

  GetPlayerSession()->emotion = emotion;
}

void Overworld::OnlineArea::receiveMoneySignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  int balance = reader.Read<int>(buffer);
  GetPlayerSession()->money = balance;
}

void Overworld::OnlineArea::receiveItemSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string id = reader.ReadString<uint8_t>(buffer);
  std::string name = reader.ReadString<uint8_t>(buffer);
  std::string description = reader.ReadString<uint16_t>(buffer);

  AddItem(id, name, description);
}

void Overworld::OnlineArea::receiveRemoveItemSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string id = reader.ReadString<uint8_t>(buffer);

  RemoveItem(id);
}

void Overworld::OnlineArea::receivePlaySoundSignal(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  std::string name = reader.ReadString<uint16_t>(buffer);

  Audio().Play(GetAudio(name), AudioPriority::highest);
}

void Overworld::OnlineArea::receiveExcludeObjectSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  uint32_t objectId = reader.Read<uint32_t>(buffer);

  if (excludedObjects.find(objectId) != excludedObjects.end()) {
    return;
  }

  Map& map = GetMap();

  for (size_t i = 0; i < map.GetLayerCount(); i++) {
    Map::Layer& layer = map.GetLayer(i);

    MaybeTileObject optional_object_ref = layer.GetTileObject(objectId);

    if (optional_object_ref) {
      TileObjectRef object_ref = *optional_object_ref;
      TileObject& object = object_ref.get();

      ExcludedObjectData excludedData{};
      excludedData.visible = object.visible;
      excludedData.solid = object.solid;

      excludedObjects.emplace(objectId, excludedData);

      object.visible = false;
      object.solid = false;
      break;
    }
  }
}

void Overworld::OnlineArea::receiveIncludeObjectSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  uint32_t objectId = reader.Read<uint32_t>(buffer);
  Map& map = GetMap();

  if (excludedObjects.erase(objectId) == 0) {
    return;
  }

  for (size_t i = 0; i < map.GetLayerCount(); i++) {
    Map::Layer& layer = map.GetLayer(i);

    MaybeTileObject optional_object_ref = layer.GetTileObject(objectId);

    if (optional_object_ref) {
      TileObjectRef object_ref = *optional_object_ref;
      TileObject& object = object_ref.get();

      object.visible = true;
      object.solid = true;
      break;
    }
  }
}

void Overworld::OnlineArea::receiveExcludeActorSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string user = reader.ReadString<uint16_t>(buffer);

  // store this user id in case their actor is added after this signal
  // or the actor leaves the area and returns
  excludedActors.emplace(user);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;

  // removing the sprite instead of hiding it, to avoid teleport controller revealing the actor on us
  this->RemoveSprite(abstractUser.actor);
  this->RemoveSprite(abstractUser.teleportController.GetBeam());

  // prevent collisions with this actor
  abstractUser.actor->SetSolid(false);

  if (abstractUser.marker) {
    abstractUser.marker->Hide();
  }
}

void Overworld::OnlineArea::receiveIncludeActorSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto user = reader.ReadString<uint16_t>(buffer);

  excludedActors.erase(user);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;

  // remove this actor to make sure they're not added more than once (server script issue)
  this->RemoveSprite(abstractUser.actor);
  this->RemoveSprite(abstractUser.teleportController.GetBeam());

  // enable collisions if they should be on
  abstractUser.actor->SetSolid(abstractUser.solid);

  // include the actor again
  this->AddSprite(abstractUser.actor);
  this->AddSprite(abstractUser.teleportController.GetBeam());

  if (abstractUser.marker) {
    abstractUser.marker->Reveal();
  }
}

void Overworld::OnlineArea::receiveMoveCameraSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  Map& map = GetMap();
  sf::Vector2i tileSize = map.GetTileSize();

  float x = reader.Read<float>(buffer) * tileSize.x / 2.0f;
  float y = reader.Read<float>(buffer) * tileSize.y;
  float z = reader.Read<float>(buffer);

  sf::Vector2f position = sf::Vector2f(x, y);

  sf::Vector2f screenPos = map.WorldToScreen(position);
  screenPos.y -= z * tileSize.y / 2.0f;

  float duration = reader.Read<float>(buffer);

  serverCameraController.QueuePlaceCamera(screenPos, sf::seconds(duration));
}

void Overworld::OnlineArea::receiveSlideCameraSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  Map& map = GetMap();
  sf::Vector2i tileSize = map.GetTileSize();

  float x = reader.Read<float>(buffer) * tileSize.x / 2.0f;
  float y = reader.Read<float>(buffer) * tileSize.y;
  float z = reader.Read<float>(buffer);

  sf::Vector2f position = sf::Vector2f(x, y);

  sf::Vector2f screenPos = map.WorldToScreen(position);
  screenPos.y -= z * tileSize.y / 2.0f;

  float duration = reader.Read<float>(buffer);

  serverCameraController.QueueMoveCamera(screenPos, sf::seconds(duration));
}

void Overworld::OnlineArea::receiveShakeCameraSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  float strength = reader.Read<float>(buffer);
  sf::Time duration = sf::seconds(reader.Read<float>(buffer));

  if (serverCameraController.IsQueueEmpty()) {
    // adding shake to the queue can break tracking players
    // no need to add it to the queue if it's empty, just apply directly
    GetCamera().ShakeCamera(strength, duration);
  }
  else {
    serverCameraController.QueueShakeCamera(strength, duration);
  }
}

void Overworld::OnlineArea::receiveFadeCameraSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  sf::Time duration = sf::seconds(reader.Read<float>(buffer));

  uint8_t rgba[4];

  rgba[0] = reader.Read<uint8_t>(buffer);
  rgba[1] = reader.Read<uint8_t>(buffer);
  rgba[2] = reader.Read<uint8_t>(buffer);
  rgba[3] = reader.Read<uint8_t>(buffer);

  if (serverCameraController.IsQueueEmpty()) {
    GetCamera().FadeCamera(sf::Color(rgba[0], rgba[1], rgba[2], rgba[3]), duration);
  }
  else {
    serverCameraController.QueueFadeCamera(sf::Color(rgba[0], rgba[1], rgba[2], rgba[3]), duration);
  }
}

void Overworld::OnlineArea::receiveTrackWithCameraSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  bool some = reader.Read<bool>(buffer);

  if (some) {
    trackedPlayer = reader.ReadString<uint16_t>(buffer);
  }
  else {
    trackedPlayer = {};
  }
}

void Overworld::OnlineArea::receiveEnableCameraControls(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  float dist_x = reader.Read<float>(buffer);
  float dist_y = reader.Read<float>(buffer);

  EnableCameraControls(dist_x, dist_y);
}

void Overworld::OnlineArea::receiveTeleportSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  // todo: prevent warp animation for other clients if warp = false?
  // maybe add a warped signal we send to the server?

  bool warp = reader.Read<bool>(buffer);
  float x = reader.Read<float>(buffer);
  float y = reader.Read<float>(buffer);
  float z = reader.Read<float>(buffer);
  Direction direction = reader.Read<Direction>(buffer);

  sf::Vector2i tileSize = GetMap().GetTileSize();
  sf::Vector3f position = sf::Vector3f(
    x * tileSize.x / 2.0f,
    y * tileSize.y,
    z
  );

  ActorPtr player = GetPlayer();

  if (warp) {
    auto& action = GetTeleportController().TeleportOut(player);
    action.onFinish.Slot([this, player, position, direction] {
      teleportIn(position, Orthographic(direction));
    });
  }
  else {
    player->Face(Orthographic(direction));
    player->Set3DPosition(position);
  }
}

void Overworld::OnlineArea::receiveMessageSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto message = reader.ReadString<uint16_t>(buffer);
  auto mugTexturePath = reader.ReadString<uint16_t>(buffer);
  auto mugAnimationPath = reader.ReadString<uint16_t>(buffer);

  sf::Sprite face;
  face.setTexture(*GetTexture(mugTexturePath));

  Animation animation;
  animation.LoadWithData(GetText(mugAnimationPath));

  auto& menuSystem = GetMenuSystem();
  menuSystem.SetNextSpeaker(face, animation);
  menuSystem.EnqueueMessage(message,
    [=]() { sendTextBoxResponseSignal(0); }
  );
}

void Overworld::OnlineArea::receiveQuestionSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto message = reader.ReadString<uint16_t>(buffer);
  auto mugTexturePath = reader.ReadString<uint16_t>(buffer);
  auto mugAnimationPath = reader.ReadString<uint16_t>(buffer);

  sf::Sprite face;
  face.setTexture(*GetTexture(mugTexturePath));

  Animation animation;
  animation.LoadWithData(GetText(mugAnimationPath));

  auto& menuSystem = GetMenuSystem();
  menuSystem.SetNextSpeaker(face, animation);
  menuSystem.EnqueueQuestion(message,
    [=](int response) { sendTextBoxResponseSignal((char)response); }
  );
}

void Overworld::OnlineArea::receiveQuizSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto optionA = reader.ReadString<uint16_t>(buffer);
  auto optionB = reader.ReadString<uint16_t>(buffer);
  auto optionC = reader.ReadString<uint16_t>(buffer);
  auto mugTexturePath = reader.ReadString<uint16_t>(buffer);
  auto mugAnimationPath = reader.ReadString<uint16_t>(buffer);

  sf::Sprite face;
  face.setTexture(*GetTexture(mugTexturePath));
  Animation animation;

  animation.LoadWithData(GetText(mugAnimationPath));

  auto& menuSystem = GetMenuSystem();
  menuSystem.SetNextSpeaker(face, animation);
  menuSystem.EnqueueQuiz(optionA, optionB, optionC,
    [=](int response) { sendTextBoxResponseSignal((char)response); }
  );
}

void Overworld::OnlineArea::receivePromptSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto characterLimit = reader.Read<uint16_t>(buffer);
  auto defaultText = reader.ReadString<uint16_t>(buffer);

  auto& menuSystem = GetMenuSystem();
  menuSystem.EnqueueTextInput(
    defaultText,
    characterLimit,
    [=](auto& response) { sendPromptResponseSignal(response); }
  );
}

static std::vector<BBS::Post> ReadPosts(BufferReader& reader, const Poco::Buffer<char>& buffer) {
  auto total = reader.Read<uint16_t>(buffer);

  std::vector<BBS::Post> posts;
  posts.reserve(total);

  for (auto i = 0; i < total; i++) {
    auto id = reader.ReadString<uint16_t>(buffer);
    auto read = reader.Read<bool>(buffer);
    auto title = reader.ReadString<uint16_t>(buffer);
    auto author = reader.ReadString<uint16_t>(buffer);

    posts.push_back({
      id,
      read,
      title,
      author
      });
  }

  return posts;
}

void Overworld::OnlineArea::receiveOpenBoardSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& menuSystem = GetMenuSystem();

  auto topic = reader.ReadString<uint16_t>(buffer);
  auto r = reader.Read<unsigned char>(buffer);
  auto g = reader.Read<unsigned char>(buffer);
  auto b = reader.Read<unsigned char>(buffer);
  auto openInstantly = reader.Read<bool>(buffer);
  auto posts = ReadPosts(reader, buffer);

  menuSystem.OpenBBS(
    topic,
    sf::Color(r, g, b, 255),
    openInstantly,
    [=](auto id) { sendPostSelectSignal(id); },
    [=] { sendBoardCloseSignal(); }
  );

  sendBoardOpenSignal();

  auto& bbs = menuSystem.GetBBS()->get();
  bbs.AppendPosts(posts);

  bbs.SetLastPageCallback([=] { sendPostRequestSignal(); });
}

void Overworld::OnlineArea::receivePrependPostsSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& menuSystem = GetMenuSystem();

  bool hasReference = reader.Read<bool>(buffer);
  std::string reference = hasReference ? reader.ReadString<uint16_t>(buffer) : "";
  auto posts = ReadPosts(reader, buffer);

  auto optionalBbs = menuSystem.GetBBS();

  if (!optionalBbs) {
    return;
  }

  auto& bbs = optionalBbs->get();

  if (!hasReference) {
    bbs.PrependPosts(posts);
  }
  else {
    bbs.PrependPosts(reference, posts);
  }
}

void Overworld::OnlineArea::receiveAppendPostsSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& menuSystem = GetMenuSystem();

  auto hasReference = reader.Read<bool>(buffer);
  auto reference = hasReference ? reader.ReadString<uint16_t>(buffer) : "";
  auto posts = ReadPosts(reader, buffer);

  auto optionalBbs = menuSystem.GetBBS();

  if (!optionalBbs) {
    return;
  }

  auto& bbs = optionalBbs->get();

  if (!hasReference) {
    bbs.AppendPosts(posts);
  }
  else {
    bbs.AppendPosts(reference, posts);
  }
}

void Overworld::OnlineArea::receiveRemovePostSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& menuSystem = GetMenuSystem();

  auto postId = reader.ReadString<uint16_t>(buffer);

  auto optionalBbs = menuSystem.GetBBS();

  if (!optionalBbs) {
    return;
  }

  auto& bbs = optionalBbs->get();

  bbs.RemovePost(postId);
}

void  Overworld::OnlineArea::receiveCloseBBSSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  GetMenuSystem().CloseBBS();
}

void Overworld::OnlineArea::receiveShopInventorySignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto itemCount = reader.Read<uint16_t>(buffer);

  for (auto i = 0; i < itemCount; i++) {
    VendorScene::Item item;
    item.name = reader.ReadString<uint8_t>(buffer);
    item.desc = reader.ReadString<uint8_t>(buffer);
    item.cost = reader.Read<uint32_t>(buffer);

    shopItems.push_back(item);
  }
}

void Overworld::OnlineArea::receiveOpenShopSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  uint32_t money = GetPlayerSession()->money;

  auto mugTexturePath = reader.ReadString<uint16_t>(buffer);
  auto mugAnimationPath = reader.ReadString<uint16_t>(buffer);

  std::vector<VendorScene::Item> shopItemCopy;
  std::swap(shopItems, shopItemCopy);

  AddSceneChangeTask([=] {
    auto mugTexture = GetTexture(mugTexturePath);

    Animation mugAnimation;
    mugAnimation.LoadWithData(GetText(mugAnimationPath));

    auto callback = [this](const std::string& itemName) {
      sendShopPurchaseSignal(itemName);
    };

    returningFrom = ReturningScene::VendorScene;
    getController().push<segue<BlackWashFade>::to<VendorScene>>(shopItemCopy, money, mugTexture, mugAnimation, callback);
  });
}

void Overworld::OnlineArea::receivePVPSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string remoteAddress = reader.ReadString<uint16_t>(buffer);
  Poco::Net::SocketAddress remote = Poco::Net::SocketAddress(remoteAddress);

  BlockPackagePartitioner& blockPartition = getController().BlockPackagePartitioner();
  CardPackagePartitioner& cardPartition = getController().CardPackagePartitioner();
  PlayerPackagePartitioner& playerPartition = getController().PlayerPackagePartitioner();

  try {
    netBattleProcessor = std::make_shared<Netplay::PacketProcessor>(remote, Net().GetMaxPayloadSize());
    Net().AddHandler(remote, netBattleProcessor);
  }
  catch (...) {
    Logger::Log(LogLevel::critical, "Failed to connect to remote player");
    ResetPVPStep(true);
    return;
  }

  AddSceneChangeTask([&] {
    // adding as a scene change task to block one frame
    copyScreen = true; // copy screen contents for download screen fx
  });

  AddSceneChangeTask([=, &blockPartition, &playerPartition] {
    CardPackagePartitioner& cardPartition = getController().CardPackagePartitioner();
    std::vector<PackageHash> cards, selectedNaviBlocks;
    const std::string& selectedNaviId = GetCurrentNaviID();
    std::optional<CardFolder*> selectedFolder = GetSelectedFolder();

    if (selectedFolder && getController().Session().IsFolderAllowed(*selectedFolder)) {
      std::unique_ptr<CardFolder> folder = (*selectedFolder)->Clone();

      Battle::Card* next = folder->Next();

      while (next) {
        const std::string& packageId = next->GetUUID();
        PackageAddress packageAddr = PackageAddress{ Game::LocalPartition, packageId };
        if (cardPartition.HasPackage(packageAddr)) {
          const std::string& md5 = cardPartition.FindPackageByAddress(packageAddr).GetPackageFingerprint();
          cards.push_back({ next->GetUUID(), md5 });
        }
        next = folder->Next();
      }
    }

    GameSession& session = getController().Session();
    for (const PackageAddress& blockAddr : PlayerCustScene::GetInstalledBlocks(selectedNaviId, session)) {
      const std::string& blockId = blockAddr.packageId;
      if (!blockPartition.HasPackage(blockAddr)) continue;
      const std::string& md5 = blockPartition.FindPackageByAddress(blockAddr).GetPackageFingerprint();
      selectedNaviBlocks.push_back({ blockId, md5 });
    }

    PackageAddress playerPackageAddr = PackageAddress{ Game::LocalPartition, selectedNaviId };
    const std::string& playerPackageMd5 = playerPartition.FindPackageByAddress(playerPackageAddr).GetPackageFingerprint();
    PackageHash playerPackageHash = { GetCurrentNaviID(), playerPackageMd5 };

    DownloadSceneProps props = {
      cards,
      selectedNaviBlocks,
      playerPackageHash,
      remote,
      netBattleProcessor,
      screen,
      this->canProceedToBattle,
      this->pvpCoinFlip,
      this->remoteNaviPackage,
      this->remoteNaviBlocks
    };

    returningFrom = ReturningScene::DownloadScene;
    using effect = swoosh::types::segue<VerticalOpen, milliseconds<500>>;
    getController().push<effect::to<DownloadScene>>(props);
  });

  AddSceneChangeTask([=, &blockPartition, &playerPartition] {
    if (!this->canProceedToBattle) {
      Logger::Log(LogLevel::critical, "Failed to download assets from remote player");
      return;
    }

    std::optional<CardFolder*> selectedFolder = GetSelectedFolder();
    std::unique_ptr<CardFolder> folder;

    if (selectedFolder && getController().Session().IsFolderAllowed(*selectedFolder)) {
      folder = (*selectedFolder)->Clone();
      folder->Shuffle();
    }
    else {
      // use a new blank folder if we dont have a proper folder selected
      folder = std::make_unique<CardFolder>();
    }

    // Play the pre battle sound
    Audio().Play(AudioType::PRE_BATTLE, AudioPriority::high);

    // Stop music and go to battle screen
    Audio().StopStream();

    PackageAddress playerPackage = PackageAddress{ Game::LocalPartition , GetCurrentNaviID() };
    auto& meta = playerPartition.FindPackageByAddress(playerPackage);
    std::filesystem::path mugshotAnim = meta.GetMugshotAnimationPath();
    auto mugshot = Textures().LoadFromFile(meta.GetMugshotTexturePath());
    auto emotions = Textures().LoadFromFile(meta.GetEmotionsTexturePath());
    auto player = std::shared_ptr<Player>(meta.GetData());

    auto& overworldSession = GetPlayerSession();
    player->SetMaxHealth(overworldSession->maxHealth);
    player->SetHealth(overworldSession->health);
    player->SetEmotion(overworldSession->emotion);

    GameSession& session = getController().Session();
    std::vector<PackageAddress> localNaviBlocks = PlayerCustScene::GetInstalledBlocks(GetCurrentNaviID(), session);

    auto& remoteMeta = playerPartition.FindPackageByAddress(remoteNaviPackage);
    auto remotePlayer = std::shared_ptr<Player>(remoteMeta.GetData());

    std::vector<NetworkPlayerSpawnData> spawnOrder;
    spawnOrder.push_back({ localNaviBlocks, player });
    spawnOrder.push_back({ remoteNaviBlocks, remotePlayer });

    // Make player who can go first the priority in the list
    std::iter_swap(spawnOrder.begin(), spawnOrder.begin() + this->pvpCoinFlip);

    // red team goes first
    spawnOrder[0].x = 2;
    spawnOrder[0].y = 2;

    // blue team goes second
    spawnOrder[1].x = 5;
    spawnOrder[1].y = 2;

    NetworkBattleSceneProps props = {
      { player, GetProgramAdvance(), std::move(folder), std::make_shared<Field>(6, 3), GetBackground() },
      sf::Sprite(*mugshot),
      mugshotAnim,
      emotions,
      netBattleProcessor,
      spawnOrder
    };

    BattleResultsFunc callback = [this](const BattleResults& results) {
      sendBattleResultsSignal(results);
    };

    returningFrom = ReturningScene::BattleScene;
    getController().push<segue<WhiteWashFade>::to<NetworkBattleScene>>(props, callback);
  });
}

template<typename Partitioner>
static void LoadPackage(Partitioner& partitioner, const std::filesystem::path& file_path) {
  auto& packageManager = partitioner.GetPartition(Game::ServerPartition);

  std::string packageId = packageManager.FilepathToPackageID(file_path);

  if (!packageId.empty() && packageManager.HasPackage(packageId)) {
    return;
  }

  // install for the first time
  auto res = packageManager.template LoadPackageFromZip<ScriptedMob>(file_path);

  if (res.is_error()) {
    Logger::Logf(LogLevel::critical, "Error loading server package %s: %s", packageId.c_str(), res.error_cstr());
  }

  Logger::Logf(LogLevel::info, "Installed server package: %s", res.value().c_str());
}

void Overworld::OnlineArea::receiveLoadPackageSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  if (transferringServers) {
    // skip loading packages if we're leaving anyway
    return;
  }

  std::string asset_path = reader.ReadString<uint16_t>(buffer);
  PackageType package_type = reader.Read<PackageType>(buffer);

  std::filesystem::path file_path = serverAssetManager.GetPath(asset_path);

  if (file_path.empty()) {
    Logger::Logf(LogLevel::critical, "Failed to find server asset %s", file_path.c_str());
    return;
  }

  switch (package_type) {
  case PackageType::blocks:
    LoadPackage(getController().BlockPackagePartitioner(), file_path);
    break;
  case PackageType::card:
    LoadPackage(getController().CardPackagePartitioner(), file_path);
    break;
  case PackageType::library:
    LoadPackage(getController().LuaLibraryPackagePartitioner(), file_path);
    break;
  case PackageType::player:
    LoadPackage(getController().PlayerPackagePartitioner(), file_path);
    break;
  default:
    LoadPackage(getController().MobPackagePartitioner(), file_path);
  }
}

template <typename ScriptedType, typename Manager>
void Overworld::OnlineArea::InstallPackage(Manager& manager, const std::filesystem::path& modFolder, const std::string& packageName, const std::string& packageId, const std::filesystem::path& filePath) {
  auto& menuSystem = GetMenuSystem();

  SetAvatarAsSpeaker();
  menuSystem.EnqueueMessage("Installing\x01...");

  manager.ErasePackage(packageId);

  std::filesystem::path installPath = modFolder / ("package-" + URIEncode(packageId) + ".zip");

  try {
    std::filesystem::copy_file(filePath, installPath);
  } catch(std::exception& e) {
    Logger::Logf(LogLevel::critical, "Failed to copy package %s to %s. Reason: %s", packageId.c_str(), installPath.c_str(), e.what());
    menuSystem.EnqueueMessage("Installation failed.");
    return;
  }

  auto res = manager.template LoadPackageFromZip<ScriptedType>(installPath);

  if (res.is_error()) {
    Logger::Logf(LogLevel::critical, "%s", res.error_cstr());
    menuSystem.EnqueueMessage("Installation failed.");
    return;
  }

  menuSystem.EnqueueMessage(packageName + " successfully installed!");
}

template <typename ScriptedType, typename Partitioner>
void Overworld::OnlineArea::RunPackageWizard(Partitioner& partitioner, const std::filesystem::path& modFolder, const std::string& packageName, const std::string& packageId, const std::filesystem::path& filePath)
{
  auto& localManager = partitioner.GetPartition(Game::LocalPartition);

  bool hasPackage = localManager.HasPackage(packageId);

  // check if the package is already installed
  if (localManager.HasPackage(packageId)) {
    stx::result_t<std::string> md5Result = stx::generate_md5_from_file(filePath);

    if (md5Result.is_error()) {
      Logger::Logf(LogLevel::critical, "Failed to create md5 for %s. Reason: %s", filePath.c_str(), md5Result.error_cstr());
      return;
    }

    std::string md5 = md5Result.value();

    if (localManager.FindPackageByID(packageId).fingerprint == md5) {
      // package already installed
      return;
    }
  }

  // request permission from the player
  SetAvatarAsSpeaker();

  GetMenuSystem().EnqueueMessage("Receiving data\x01...", [this]() {
    GetPlayer()->Face(Direction::down_right);
  });

  GetMenuSystem().EnqueueQuestion(
    "Received data for " + packageName + " install?",
    [this, &localManager, hasPackage, modFolder, packageName, packageId, filePath](bool yes) {
      if (!yes) {
        return;
      }

      if (!hasPackage) {
        InstallPackage<ScriptedType>(localManager, modFolder, packageName, packageId, filePath);
        return;
      }

      SetAvatarAsSpeaker();
      GetMenuSystem().EnqueueQuestion(
        packageName + " conflicts with an existing package, overwrite?",
        [this, &localManager, modFolder, packageName, packageId, filePath](bool yes) {
          if (!yes) {
            return;
          }

          InstallPackage<ScriptedType>(localManager, modFolder, packageName, packageId, filePath);
        }
      );
    }
  );
}

void Overworld::OnlineArea::RunPackageWizard(PackageType packageType, const std::string& packageName, std::string& packageId, const std::filesystem::path& filePath)
{
  // todo: define mod folders in a single location?

  switch (packageType) {
  case PackageType::blocks:
    RunPackageWizard<ScriptedBlock>(getController().BlockPackagePartitioner(), "resources/mods/blocks", packageName, packageId, filePath);
    break;
  case PackageType::card:
    RunPackageWizard<ScriptedCard>(getController().CardPackagePartitioner(), "resources/mods/cards", packageName, packageId, filePath);
    break;
  case PackageType::library:
    RunPackageWizard<LuaLibrary>(getController().LuaLibraryPackagePartitioner(), "resources/mods/libs", packageName, packageId, filePath);
    break;
  case PackageType::player:
    RunPackageWizard<ScriptedPlayer>(getController().PlayerPackagePartitioner(), "resources/mods/players", packageName, packageId, filePath);
    break;
  default:
    RunPackageWizard<ScriptedMob>(getController().MobPackagePartitioner(), "resources/mods/enemies", packageName, packageId, filePath);
  }
}

void Overworld::OnlineArea::receivePackageOfferSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  PackageType packageType = reader.Read<PackageType>(buffer);
  std::string packageId = reader.ReadString<uint8_t>(buffer);
  std::string packageName = reader.ReadString<uint8_t>(buffer);
  std::filesystem::path filePath = GetPath(reader.ReadString<uint16_t>(buffer));

  if (packageName.empty()) {
    packageName = "Dependency";
  }

  RunPackageWizard(packageType, packageName, packageId, filePath);
}

static std::vector<PackageHash> ParsePackageList(std::string_view packageListView) {
  std::vector<PackageHash> packageHashes;

  size_t endLine = 0;

  do {
    size_t startLine = endLine;
    endLine = packageListView.find("\n", startLine);

    if (endLine == string::npos) {
      endLine = packageListView.size();
    }

    std::string_view lineView = packageListView.substr(startLine, endLine - startLine);
    endLine += 1; // skip past the \n

    if (lineView[lineView.size() - 1] == '\r') {
      lineView = lineView.substr(0, lineView.size() - 1);
    }

    if (lineView.size() <= 32) {
      // not enough for md5 + package id
      continue;
    }

    size_t spaceIndex = lineView.find(' ');

    if (spaceIndex == string::npos) {
      // missing space
      continue;
    }

    PackageHash packageHash;
    packageHash.md5 = lineView.substr(0, 32);
    packageHash.packageId = lineView.substr(33);

    packageHashes.push_back(packageHash);
  } while(endLine < packageListView.size());

  return packageHashes;
}

void Overworld::OnlineArea::receiveModWhitelistSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string assetPath = reader.ReadString<uint16_t>(buffer);
  std::string whitelistString = assetPath.empty() ? "" : GetText(assetPath);
  std::vector<PackageHash> packageHashes = ParsePackageList(whitelistString);

  getController().Session().SetWhitelist(packageHashes);

  AddSceneChangeTask([this] { CheckPlayerAgainstWhitelist(); });
}

void Overworld::OnlineArea::receiveModBlacklistSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string assetPath = reader.ReadString<uint16_t>(buffer);
  std::string blacklistString = assetPath.empty() ? "" : GetText(assetPath);
  std::vector<PackageHash> packageHashes = ParsePackageList(blacklistString);

  // getController().Session().SetBlacklist(packageHashes);

  // AddSceneChangeTask([this] { CheckPlayerAgainstWhitelist(); });
}

void Overworld::OnlineArea::receiveMobSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  if (transferringServers) {
    sendBattleResultsSignal(BattleResults{});
    // ignore encounters if we're leaving
    return;
  }

  std::string asset_name = reader.ReadString<uint16_t>(buffer);
  std::string data_path = reader.ReadString<uint16_t>(buffer);

  std::filesystem::path file_path = serverAssetManager.GetPath(asset_name);

  if (file_path.empty()) {
    Logger::Logf(LogLevel::critical, "Failed to find server mob asset %s", file_path.c_str());
    return;
  }

  MobPackageManager& mobPackages = getController().MobPackagePartitioner().GetPartition(Game::ServerPartition);
  PlayerPackageManager& playerPackages = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);

  std::string packageId = mobPackages.FilepathToPackageID(file_path);

  if (!mobPackages.HasPackage(packageId)) {
    // If we don't have it by now something went terribly wrong
    Logger::Logf(LogLevel::critical, "Failed to battle server mob package %s", file_path.c_str());
    return;
  }

  Logger::Logf(LogLevel::debug, "Battling server mob %s", packageId.c_str());

  MobMeta& mobMeta = mobPackages.FindPackageByID(packageId);

  std::unique_ptr<MobFactory> mobFactory = std::unique_ptr<MobFactory>(mobMeta.GetData());
  Mob* mob = mobFactory->Build(std::make_shared<Field>(6, 3), GetText(data_path));

  AddSceneChangeTask([=, &playerPackages, &mobPackages] {
    // Play the pre battle rumble sound
    Audio().Play(AudioType::PRE_BATTLE, AudioPriority::high);

    // Stop music and go to battle screen
    Audio().StopStream();

    // Get the navi we selected
    PlayerMeta& playerMeta = playerPackages.FindPackageByID(GetCurrentNaviID());
    std::filesystem::path mugshotAnim = playerMeta.GetMugshotAnimationPath();
    std::shared_ptr<sf::Texture> mugshot = Textures().LoadFromFile(playerMeta.GetMugshotTexturePath());
    std::shared_ptr<sf::Texture> emotions = Textures().LoadFromFile(playerMeta.GetEmotionsTexturePath());
    std::shared_ptr<Player> player = std::shared_ptr<Player>(playerMeta.GetData());

    auto& overworldSession = GetPlayerSession();
    player->SetMaxHealth(overworldSession->maxHealth);
    player->SetHealth(overworldSession->health);
    player->SetEmotion(overworldSession->emotion);

    CardFolder* newFolder = nullptr;

    std::optional<CardFolder*> selectedFolder = GetSelectedFolder();
    std::unique_ptr<CardFolder> folder;

    GameSession& gameSession = getController().Session();
    if (selectedFolder && gameSession.IsFolderAllowed(*selectedFolder)) {
      folder = (*selectedFolder)->Clone();
      folder->Shuffle();
    }
    else {
      // use a new blank folder if we dont have a proper folder selected
      folder = std::make_unique<CardFolder>();
    }

    // Queue screen transition to Battle Scene with a white fade effect
    // just like the game
    if (!mob->GetBackground()) {
      mob->SetBackground(GetBackground());
    }

    std::vector<PackageAddress> localNaviBlocksAddr = PlayerCustScene::GetInstalledBlocks(playerMeta.packageId, gameSession);
    std::vector<std::string> localNaviBlocks;

    for (const PackageAddress& addr : localNaviBlocksAddr) {
      localNaviBlocks.push_back(addr.packageId);
    }

    BattleResultsFunc callback = [this](const BattleResults& results) {
      sendBattleResultsSignal(results);
    };

    // Queue screen transition to Battle Scene with a white fade effect
    // just like the game
    if (mob->IsFreedomMission()) {
      FreedomMissionProps props{
        { player, GetProgramAdvance(), std::move(folder), mob->GetField(), mob->GetBackground() },
        { mob },
        mob->GetTurnLimit(),
        sf::Sprite(*mugshot),
        mugshotAnim,
        emotions,
        localNaviBlocks
      };

      using effect = segue<WhiteWashFade>;
      getController().push<effect::to<FreedomMissionMobScene>>(std::move(props), callback);
    }
    else {
      MobBattleProperties props{
        { player, GetProgramAdvance(), std::move(folder), mob->GetField(), mob->GetBackground() },
        MobBattleProperties::RewardBehavior::take,
        { mob },
        sf::Sprite(*mugshot),
        mugshotAnim,
        emotions,
        localNaviBlocks
      };

      using effect = segue<WhiteWashFade>;
      getController().push<effect::to<MobBattleScene>>(std::move(props), callback);
    }

    GetPlayer()->Face(GetPlayer()->GetHeading());
    returningFrom = ReturningScene::BattleScene;
  });
}

void Overworld::OnlineArea::receiveActorConnectedSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto& map = GetMap();
  auto tileSize = sf::Vector2f(map.GetTileSize());

  std::string user = reader.ReadString<uint16_t>(buffer);
  std::string name = reader.ReadString<uint16_t>(buffer);
  std::string texturePath = reader.ReadString<uint16_t>(buffer);
  std::string animationPath = reader.ReadString<uint16_t>(buffer);
  auto direction = reader.Read<Direction>(buffer);
  float x = reader.Read<float>(buffer) * tileSize.x / 2.0f;
  float y = reader.Read<float>(buffer) * tileSize.y;
  float z = reader.Read<float>(buffer);
  bool solid = reader.Read<bool>(buffer);
  bool warpIn = reader.Read<bool>(buffer);
  float scaleX = reader.Read<float>(buffer);
  float scaleY = reader.Read<float>(buffer);
  float rotation = reader.Read<float>(buffer);
  sf::Color minimapColor = reader.ReadRGBA(buffer);

  std::optional<std::string> current_animation;

  if (reader.Read<bool>(buffer)) {
    current_animation = reader.ReadString<uint16_t>(buffer);
  }

  auto pos = sf::Vector3f(x, y, z);

  if (user == ticket) return;

  // ignore success, update if this player already exists
  auto [pair, isNew] = onlinePlayers.emplace(user, name);

  auto& ticket = pair->first;
  auto& onlinePlayer = pair->second;

  // cancel possible removal since we're trying to add this user back
  removePlayers.remove(user);
  onlinePlayer.disconnecting = false;

  // update
  onlinePlayer.timestamp = GetSteadyTime();
  onlinePlayer.lagWindow.SetSmoothing(ROLLING_WINDOW_SMOOTHING);
  onlinePlayer.lagWindow.Push(SECONDS_PER_MOVEMENT); // initialize the average to the expected spacing
  onlinePlayer.startBroadcastPos = pos;
  onlinePlayer.endBroadcastPos = pos;
  onlinePlayer.idleDirection = Orthographic(direction);
  onlinePlayer.propertyAnimator.ToggleAudio(false);

  auto actor = onlinePlayer.actor;
  actor->Set3DPosition(pos);
  actor->Face(onlinePlayer.idleDirection);
  actor->setTexture(GetTexture(texturePath));
  actor->scale(scaleX, scaleY);
  actor->rotate(rotation);

  Animation animation;
  animation.LoadWithData(GetText(animationPath));
  actor->LoadAnimations(animation);

  if (current_animation) {
    actor->PlayAnimation(*current_animation, true);
  }

  auto& emoteNode = onlinePlayer.emoteNode;
  emoteNode = std::make_shared<Overworld::EmoteNode>();

  // move the emote above the player's head
  RefreshEmotePosition(*emoteNode, *actor);
  emoteNode->setScale(0.5f, 0.5f);
  emoteNode->LoadCustomEmotes(customEmotesTexture);

  onlinePlayer.solid = solid;
  actor->SetSolid(solid);

  auto& teleportController = onlinePlayer.teleportController;

  auto isExcluded = excludedActors.find(user) != excludedActors.end();

  if (isNew) {
    // add nodes to the scene base
    teleportController.EnableSound(false);

    actor->AddNode(emoteNode);
    actor->CollideWithMap(false);
    actor->SetCollisionRadius(6);
    actor->SetInteractCallback([=](const std::shared_ptr<Actor>& with, Interaction type) {
      if (excludedActors.find(user) == excludedActors.end()) {
        sendNaviInteractionSignal(ticket, type);
      }
    });

    AddActor(actor);

    auto marker = std::make_shared<Minimap::PlayerMarker>(minimapColor);
    auto isConcealed = map.IsConcealed(sf::Vector2i(x, y), (int)z);
    auto& minimap = GetMinimap();
    minimap.AddPlayerMarker(marker);
    minimap.UpdatePlayerMarker(*marker, map.WorldToScreen(pos), isConcealed);
    onlinePlayer.marker = marker;

    if (isExcluded) {
      // remove the actor if they are marked as hidden by the server
      RemoveSprite(actor);
      marker->Hide();
      actor->SetSolid(false);
    }
    else {
      // add the teleport beam if the actor is not marked as hidden by the server
      AddSprite(teleportController.GetBeam());
    }
  }

  if (warpIn) {
    teleportController.TeleportIn(actor, pos, Direction::none);
  }
}

void Overworld::OnlineArea::receiveActorDisconnectedSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string user = reader.ReadString<uint16_t>(buffer);
  bool warpOut = reader.Read<bool>(buffer);

  auto userIter = onlinePlayers.find(user);

  if (userIter == onlinePlayers.end()) {
    return;
  }

  auto& onlinePlayer = userIter->second;

  onlinePlayer.disconnecting = true;

  auto actor = onlinePlayer.actor;

  if (warpOut) {
    auto& teleport = onlinePlayer.teleportController;
    teleport.TeleportOut(actor).onFinish.Slot([=] {

      // search for the player again, in case they were already removed
      auto userIter = this->onlinePlayers.find(user);

      if (userIter == this->onlinePlayers.end()) {
        return;
      }

      auto& onlinePlayer = userIter->second;

      // make sure this player is still disconnecting
      if (onlinePlayer.disconnecting) {
        removePlayers.push_back(user);
      }
    });
  }
  else {
    removePlayers.push_back(user);
  }
}

void Overworld::OnlineArea::receiveActorSetNameSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string user = reader.ReadString<uint16_t>(buffer);
  std::string name = reader.ReadString<uint16_t>(buffer);

  auto userIter = onlinePlayers.find(user);

  if (userIter != onlinePlayers.end()) {
    userIter->second.actor->Rename(name);
  }
}

void Overworld::OnlineArea::receiveActorMoveSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string user = reader.ReadString<uint16_t>(buffer);

  // ignore our ip update
  if (user == ticket) {
    return;
  }


  auto& map = GetMap();
  auto tileSize = sf::Vector2f(map.GetTileSize());

  float x = reader.Read<float>(buffer) * tileSize.x / 2.0f;
  float y = reader.Read<float>(buffer) * tileSize.y;
  float z = reader.Read<float>(buffer);
  auto direction = reader.Read<Direction>(buffer);

  auto userIter = onlinePlayers.find(user);

  if (userIter != onlinePlayers.end()) {
    // Calculate the NEXT frame and see if we're moving too far
    auto& onlinePlayer = userIter->second;
    auto currentTime = GetSteadyTime();
    auto endBroadcastPos = onlinePlayer.endBroadcastPos;
    auto newPos = sf::Vector3f(x, y, z);
    auto screenDelta = map.WorldToScreen(endBroadcastPos - newPos);
    float distance = Hypotenuse({ screenDelta.x, screenDelta.y });
    double timeDifference = (currentTime - static_cast<double>(onlinePlayer.timestamp)) / 1000.0;

    auto teleportController = &onlinePlayer.teleportController;
    bool animatingPos = onlinePlayer.propertyAnimator.IsAnimatingPosition();
    auto actor = onlinePlayer.actor;

    // Do not attempt to animate the teleport over quick movements if already teleporting or animating position
    if (teleportController->IsComplete() && !animatingPos) {
      auto expectedTime = onlinePlayer.lagWindow.GetEMA();

      // we can't possibly have moved this far away without teleporting
      if (distance >= (onlinePlayer.actor->GetRunSpeed() * 2.f) * expectedTime) {
        actor->Set3DPosition(endBroadcastPos);
        auto& action = teleportController->TeleportOut(actor);
        action.onFinish.Slot([=] {
          teleportController->TeleportIn(actor, newPos, Direction::none);
        });
      }
    }

    // update our records
    if (currentTime - onlinePlayer.lastMovementTime < MAX_IDLE_MS) {
      // dont include the time difference if this player was idling longer than the server rebroadcasts for
      onlinePlayer.lagWindow.Push((float)timeDifference);
    }

    if (newPos != endBroadcastPos) {
      onlinePlayer.lastMovementTime = currentTime;
    }

    auto currentDistanceToEnd = Distance(actor->getPosition(), ToVector2f(newPos));
    auto likelyIdle = currentDistanceToEnd < MIN_IDLE_MOVEMENT;

    // use the newPos for both positions if the player is likely idle
    onlinePlayer.startBroadcastPos = likelyIdle ? newPos : actor->Get3DPosition();
    onlinePlayer.endBroadcastPos = newPos;

    onlinePlayer.timestamp = currentTime;
    onlinePlayer.idleDirection = Orthographic(direction);
  }
}

void Overworld::OnlineArea::receiveActorSetAvatarSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  std::string user = reader.ReadString<uint16_t>(buffer);
  std::string texturePath = reader.ReadString<uint16_t>(buffer);
  std::string animationPath = reader.ReadString<uint16_t>(buffer);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;
  auto& actor = abstractUser.actor;
  auto& emoteNode = abstractUser.emoteNode;

  actor->setTexture(GetTexture(texturePath));

  Animation animation;
  animation.LoadWithData(GetText(animationPath));
  actor->LoadAnimations(animation);

  RefreshEmotePosition(*emoteNode, *actor);
}

void Overworld::OnlineArea::receiveActorEmoteSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto user = reader.ReadString<uint16_t>(buffer);
  auto emote = reader.Read<uint8_t>(buffer);
  auto custom = reader.Read<bool>(buffer);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;
  auto& emoteNode = abstractUser.emoteNode;

  if (custom) {
    emoteNode->CustomEmote(emote);
  }
  else {
    emoteNode->Emote((Emotes)emote);
  }
}

void Overworld::OnlineArea::receiveActorAnimateSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto user = reader.ReadString<uint16_t>(buffer);
  auto state = reader.ReadString<uint16_t>(buffer);
  auto loop = reader.Read<bool>(buffer);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;
  abstractUser.actor->PlayAnimation(state, loop);
}

void Overworld::OnlineArea::receiveActorKeyFramesSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto user = reader.ReadString<uint16_t>(buffer);

  // resolve target
  auto actor = GetPlayer();
  auto propertyAnimator = &this->propertyAnimator;

  if (user != ticket) {
    auto userIter = onlinePlayers.find(user);

    if (userIter == onlinePlayers.end()) return;

    auto& onlinePlayer = userIter->second;

    actor = onlinePlayer.actor;
    propertyAnimator = &onlinePlayer.propertyAnimator;
  }

  // reading keyframes
  auto tail = reader.Read<bool>(buffer);
  auto keyframeCount = reader.Read<uint16_t>(buffer);

  auto tileSize = GetMap().GetTileSize();
  auto xScale = tileSize.x / 2.0f;
  auto yScale = tileSize.y;

  for (auto i = 0; i < keyframeCount; i++) {
    ActorPropertyAnimator::KeyFrame keyframe;
    keyframe.duration = reader.Read<float>(buffer);

    auto propertyCount = reader.Read<uint16_t>(buffer);

    // resolving properties for this keyframe
    for (auto j = 0; j < propertyCount; j++) {
      ActorPropertyAnimator::PropertyStep propertyStep;

      propertyStep.ease = reader.Read<Ease>(buffer);
      propertyStep.property = reader.Read<ActorProperty>(buffer);

      switch (propertyStep.property) {
      case ActorProperty::animation:
        propertyStep.stringValue = reader.ReadString<uint16_t>(buffer);
        break;
      case ActorProperty::x:
        propertyStep.value = reader.Read<float>(buffer) * xScale;
        break;
      case ActorProperty::y:
        propertyStep.value = reader.Read<float>(buffer) * yScale;
        break;
      case ActorProperty::direction:
        propertyStep.value = (float)Orthographic(reader.Read<Direction>(buffer));
        break;
      case ActorProperty::sound_effect:
        propertyStep.stringValue = GetPath(reader.ReadString<uint16_t>(buffer)).u8string();
        break;
      case ActorProperty::sound_effect_loop:
        propertyStep.stringValue = GetPath(reader.ReadString<uint16_t>(buffer)).u8string();
        break;
      default:
        propertyStep.value = reader.Read<float>(buffer);
        break;
      }

      keyframe.propertySteps.push_back(propertyStep);
    }

    propertyAnimator->AddKeyFrame(keyframe);
  }

  if (tail) {
    propertyAnimator->UseKeyFrames(*actor);
  }
}

void Overworld::OnlineArea::receiveActorMinimapColorSignal(BufferReader& reader, const Poco::Buffer<char>& buffer)
{
  auto user = reader.ReadString<uint16_t>(buffer);
  auto color = reader.ReadRGBA(buffer);

  auto optionalAbstractUser = GetAbstractUser(user);

  if (!optionalAbstractUser) {
    return;
  }

  auto abstractUser = *optionalAbstractUser;

  if (abstractUser.marker) {
    abstractUser.marker->SetMarkerColor(color);
  }
}

void Overworld::OnlineArea::leave() {
  try {
    cleanup();
  }
  catch (std::exception& e) {
    Logger::Logf(LogLevel::critical, e.what());
  }
  tryPopScene = true;
}

void Overworld::OnlineArea::cleanup() {
  for (auto& [key, processor] : authorizationProcessors) {
    Net().DropProcessor(processor);
  }
  authorizationProcessors.clear();

  if (packetProcessor) {
    sendLogoutSignal();
    Net().DropProcessor(packetProcessor);
    packetProcessor = nullptr;
  }

  RemovePackages();
  getController().Session().SetWhitelist({}); // clear the whitelist
  // getController().Session().SetBlacklist({}); // clear the blacklist

  cleanedUp = true;
}

std::string Overworld::OnlineArea::GetText(const std::string& path) {
  if (path.find("/server", 0) == 0) {
    return serverAssetManager.GetText(path);
  }
  return Overworld::SceneBase::GetText(path);
}

std::shared_ptr<sf::Texture> Overworld::OnlineArea::GetTexture(const std::string& path) {
  if (path.find("/server", 0) == 0) {
    return serverAssetManager.GetTexture(path);
  }
  return Overworld::SceneBase::GetTexture(path);
}

std::shared_ptr<sf::SoundBuffer> Overworld::OnlineArea::GetAudio(const std::string& path) {
  if (path.find("/server", 0) == 0) {
    return serverAssetManager.GetAudio(path);
  }
  return Overworld::SceneBase::GetAudio(path);
}

std::filesystem::path Overworld::OnlineArea::GetPath(const std::string& path) {
  if (path.find("/server", 0) == 0) {
    return serverAssetManager.GetPath(path);
  }

  return path;
}

void Overworld::OnlineArea::RefreshEmotePosition(EmoteNode& emoteNode, Actor& actor)
{
    auto anim = actor.GetAnim();
    sf::Vector2f emotePos = { 0, -actor.getSprite().getOrigin().y - 10 };
    if (anim.HasPoint("head")) {
        emotePos = anim.GetPoint("head") - anim.GetPoint("origin");
        emotePos.y -= 10.f;
    }

    emoteNode.setPosition(emotePos);
}