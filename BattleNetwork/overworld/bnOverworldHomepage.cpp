#include <Poco/Net/NetException.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/HostEntry.h>
#include <Segues/BlackWashFade.h>
#include "bnOverworldHomepage.h"
#include "bnOverworldOnlineArea.h"
#include "../netplay/bnNetPlayConfig.h"
#include "../bnMessage.h"
#include "../bnGameSession.h"

using namespace swoosh::types;

constexpr size_t DEFAULT_PORT = 8765;

Overworld::Homepage::Homepage(swoosh::ActivityController& controller) :
  SceneBase(controller)
{
  std::string destination_ip = getController().Session().GetKeyValue("homepage_warp:0");

  int remotePort = getController().CommandLineValue<int>("remotePort");
  host = getController().CommandLineValue<std::string>("cyberworld");

  if (host.empty()) {
    size_t colon = destination_ip.find(':', 0);

    if (colon > 0 && colon != std::string::npos) {
      host = destination_ip.substr(0, colon);
      remotePort = std::atoi(destination_ip.substr(colon + 1u).c_str());
    } else {
      host = destination_ip;
    }
  }

  if (remotePort > 0 && host.size()) {
    try {
      remoteAddress = Poco::Net::SocketAddress(host, remotePort);

      packetProcessor = std::make_shared<Overworld::PollingPacketProcessor>(
        remoteAddress,
        Net().GetMaxPayloadSize(),
        [this](ServerStatus status, size_t maxPayloadSize) { UpdateServerStatus(status, maxPayloadSize); }
      );

      Net().AddHandler(remoteAddress, packetProcessor);
    }
    catch (Poco::IOException&) {}
  }

  LoadMap(FileUtil::Read("resources/ow/maps/homepage.tmx"));

  Map& map = GetMap();
  sf::Vector3f spawnPos;
  std::optional<sf::Vector3f> statusBotSpawnOptional, destBotSpawnOptional;

  for (size_t i = 0; i < map.GetLayerCount(); i++) {
    Map::Layer& layer = map.GetLayer(i);
    float z = (float)i;

    // search for status bot while it hasn't been found
    if (!statusBotSpawnOptional) {
      MaybeShapeObject pointOptional = layer.GetShapeObject("Warp Status Bot");

      if (pointOptional) {
        ShapeObject& point = pointOptional->get();
        statusBotSpawnOptional = { point.position.x, point.position.y, z };
      }
    }

    // search for destination bot while it hasn't been found
    if (!destBotSpawnOptional) {
      MaybeShapeObject pointOptional = layer.GetShapeObject("Destination Bot");

      if (pointOptional) {
        ShapeObject& point = pointOptional->get();
        destBotSpawnOptional = { point.position.x, point.position.y, z };
      }
    }

    // search for warps
    for (TileObject& tileObject : layer.GetTileObjects()) {
      switch (tileObject.type) {
      case ObjectType::home_warp: {
        sf::Vector2f warpLayerPos = tileObject.position + map.ScreenToWorld(sf::Vector2f(0, tileObject.size.y / 2.0f));
        spawnPos = { warpLayerPos.x, warpLayerPos.y, z };
        break;
      }
      case ObjectType::server_warp: {
        sf::Vector2f centerPos = tileObject.position + map.ScreenToWorld(sf::Vector2f(0, tileObject.size.y / 2.0f));
        sf::Vector2i tileSize = map.GetTileSize();

        netWarpTilePos = sf::Vector3f(
          std::floor(centerPos.x / (float)(tileSize.x / 2)),
          std::floor(centerPos.y / tileSize.y),
          z
        );
        netWarpObjectId = tileObject.id;
        break;
      }
      default:
        break;
      }
    }
  }

  if (statusBotSpawnOptional) {
    ActorPtr mrprog = std::make_shared<Actor>("Mr. Prog");
    mrprog->LoadAnimations("resources/ow/prog/prog_ow.animation");
    mrprog->setTexture(Textures().LoadFromFile("resources/ow/prog/prog_ow.png"));
    mrprog->Set3DPosition(*statusBotSpawnOptional);
    mrprog->SetSolid(true);
    mrprog->SetCollisionRadius(5);

    // we ensure pointer to mrprog is alive because when we interact,
    // mrprog must've been alive to interact in the first place...
    mrprog->SetInteractCallback([mrprog = mrprog.get(), this](const ActorPtr& with, const Interaction& event) {
      // Face them
      mrprog->Face(*with);

      // Play message
      sf::Sprite face;
      face.setTexture(*Textures().LoadFromFile("resources/ow/prog/prog_mug.png"));

      std::string message = "If you're seeing this message, something has gone horribly wrong with the next area.";
      message += "For your safety you cannot enter the next area!";

      switch (serverStatus) {
      case ServerStatus::online:
        message = "This is your homepage! Walk into the telepad to enter cyberspace!";
        break;
      case ServerStatus::older_version:
        message = "This is your homepage! But it looks like you need to downgrade to connect to cyberspace...";
        break;
      case ServerStatus::newer_version:
        message = "This is your homepage! But it looks like you need an update to connect to cyberspace...";
        break;
      case ServerStatus::offline:
        message = "This is your homepage! But it looks like the next area is offline...";
        break;
      }

      MenuSystem& menuSystem = GetMenuSystem();
      menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
      menuSystem.EnqueueMessage(message);
    });

    AddActor(mrprog);
  }

  if (destBotSpawnOptional) {
    ActorPtr mrprog = std::make_shared<Actor>("Mr. Prog");
    mrprog->LoadAnimations("resources/ow/prog/prog_ow.animation");
    mrprog->setTexture(Textures().LoadFromFile("resources/ow/prog/prog_ow.png"));
    mrprog->Set3DPosition(destBotSpawnOptional.value());
    mrprog->SetSolid(true);
    mrprog->SetCollisionRadius(5);

    mrprog->SetInteractCallback([mrprog = mrprog.get(), this](const ActorPtr& with, const Interaction& event) {
      // Face them
      mrprog->Face(*with);

      // Play message
      sf::Sprite face;
      face.setTexture(*Textures().LoadFromFile("resources/ow/prog/prog_mug.png"));

      std::string message = "CHANGE YOUR WARP DESTINATION?";

      MenuSystem& menuSystem = GetMenuSystem();
      menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
      menuSystem.EnqueueQuestion(message, [=, &menuSystem](bool yes) {
        if (!yes) {
          return;
        }

        menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
        menuSystem.EnqueueTextInput(getController().Session().GetKeyValue("homepage_warp:0"), 128, [=, &menuSystem](const std::string& response) {
          std::string dest = response;
          size_t port = DEFAULT_PORT;

          try {
            Net().DropHandlers(remoteAddress);

            // first check if this is an ip address
            try {
              dest = Poco::Net::IPAddress::parse(dest).toString();
            }
            catch (Poco::Net::InvalidAddressException&) {
              // resolve domain name
              size_t colon = response.find(':', 0);

              if (colon > 0 && colon != std::string::npos) {
                dest = response.substr(0, colon);
                port = std::atoi(response.substr(colon + 1u).c_str());
              }

              const Poco::Net::HostEntry::AddressList addrList = Poco::Net::DNS::hostByName(dest).addresses();

              if (addrList.empty()) {
                throw std::runtime_error("Empty address list");
              }
              else {
                host = dest;
                dest = addrList.begin()->toString() + ":" + std::to_string(port);
              }
            }

            remoteAddress = Poco::Net::SocketAddress(dest);
            packetProcessor = std::make_shared<Overworld::PollingPacketProcessor>(
              remoteAddress,
              Net().GetMaxPayloadSize(),
              [this](ServerStatus status, size_t maxPayloadSize) { UpdateServerStatus(status, maxPayloadSize); }
            );
            Net().AddHandler(remoteAddress, packetProcessor);
            EnableNetWarps(false);

            menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
            menuSystem.EnqueueMessage("CHANGED TO " + response + "!");
            getController().Session().SetKeyValue("homepage_warp:0", response);
            getController().Session().SaveSession("profile.bin");
          }
          catch (...) {
            menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
            menuSystem.EnqueueMessage("SORRY I COULDN'T SET IT TO " + response + "!");
          }
        });
      });
    });

    AddActor(mrprog);
  }

  // spawn in the player
  ActorPtr player = GetPlayer();
  auto& command = GetTeleportController().TeleportIn(player, spawnPos, Direction::up, true);
  command.onFinish.Slot([=] {
    GetPlayerController().ControlActor(player);
  });
}

Overworld::Homepage::~Homepage() {
}

void Overworld::Homepage::UpdateServerStatus(ServerStatus status, uint16_t serverMaxPayloadSize) {
  serverStatus = status;
  maxPayloadSize = serverMaxPayloadSize;

  EnableNetWarps(status == ServerStatus::online);
}

void Overworld::Homepage::EnableNetWarps(bool enabled) {
  Map& map = GetMap();

  MaybeTileObject netWarpOptional = map.GetLayer(0).GetTileObject(netWarpObjectId);

  if (!netWarpOptional) {
    return;
  }

  TileObject& netWarp = netWarpOptional->get();
  TilesetPtr tileset = map.GetTileset("warp");

  netWarp.tile.gid = tileset->firstGid + (enabled ? 1 : 0);
}

void Overworld::Homepage::onUpdate(double elapsed)
{
  // Update our logic
  Map& map = GetMap();
  sf::RenderWindow& window = getController().getWindow();
  sf::Vector2i mousei = sf::Mouse::getPosition(window);
  const auto& [row, col] = PixelToRowCol(mousei, window);
  sf::Vector2f click = { (float)col * map.GetTileSize().x, (float)row * map.GetTileSize().y };

  sf::Transformable& worldTransform = GetWorldTransform();

  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Home)) {
    worldTransform.setScale(2.f, 2.f);
  }
  else if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageUp) && !scaledmap) {
    worldTransform.setScale(worldTransform.getScale() * 2.f);
    scaledmap = true;
  }
  else if (sf::Keyboard::isKeyPressed(sf::Keyboard::PageDown) && !scaledmap) {
    worldTransform.setScale(worldTransform.getScale() * 0.5f);
    scaledmap = true;
  }

  if (scaledmap
    && !sf::Keyboard::isKeyPressed(sf::Keyboard::PageUp)
    && !sf::Keyboard::isKeyPressed(sf::Keyboard::PageDown)) {
    scaledmap = false;
  }

  /*if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && !clicked) {
    const size_t tilesetCount = map.GetTilesetItemCount();
    if (tilesetCount > 0) {
      auto tile = map.GetTileAt(click);
      tile.solid = false;

      tile.ID = ((++tile.ID) % tilesetCount) + 1ull;

      map.SetTileAt(click, tile);

      clicked = true;
    }
  }
  else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right) && !clicked) {
    auto tile = map.GetTileAt(click);
    tile.solid = true;
    tile.ID = 0;

    map.SetTileAt(click, tile);

    clicked = true;
  }
  else if (clicked
    && !sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)
    && !sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
  {
    clicked = false;
  }*/

  // do default logic
  SceneBase::onUpdate(elapsed);

  if (Input().Has(InputEvents::pressed_shoulder_right) && !IsInputLocked()) {
    PlayerMeta& meta = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(GetCurrentNaviID());
    std::shared_ptr<sf::Texture> mugshot = Textures().LoadFromFile(meta.GetMugshotTexturePath());

    MenuSystem& menuSystem = GetMenuSystem();
    menuSystem.SetNextSpeaker(sf::Sprite(*mugshot), meta.GetMugshotAnimationPath());
    menuSystem.EnqueueMessage("This is your homepage.");
    menuSystem.EnqueueMessage("You can edit it anyway you like!");

    GetPlayer()->Face(Direction::down_right);
  }
}

void Overworld::Homepage::onDraw(IRenderer& renderer)
{
  SceneBase::onDraw(renderer);
}

void Overworld::Homepage::onStart()
{
  SceneBase::onStart();

  if (GetMap().GetSongPath().empty()) {
    Audio().Stream("resources/loops/loop_overworld.ogg", true);
  }
}

void Overworld::Homepage::onResume()
{
  SceneBase::onResume();

  if (GetMap().GetSongPath().empty()) {
    Audio().Stream("resources/loops/loop_overworld.ogg", true);
  }

  if (packetProcessor) {
    Net().AddHandler(remoteAddress, packetProcessor);
  }
}

void Overworld::Homepage::onLeave()
{
  SceneBase::onLeave();

  if (packetProcessor) {
    Net().DropProcessor(packetProcessor);
  }
}

void Overworld::Homepage::OnTileCollision()
{
  ActorPtr playerActor = GetPlayer();
  sf::Vector2f playerPos = playerActor->getPosition();

  Map& map = GetMap();
  sf::Vector2f tileSize = sf::Vector2f(map.GetTileSize());

  sf::Vector3f tilePos = sf::Vector3f(
    std::floor(playerPos.x / (tileSize.x / 2)),
    std::floor(playerPos.y / tileSize.y),
    std::floor(playerActor->GetElevation())
  );

  TeleportController& teleportController = GetTeleportController();

  if (netWarpTilePos == tilePos && teleportController.IsComplete() && serverStatus == ServerStatus::online) {
    PlayerController& playerController = GetPlayerController();

    // Calculate the origin by grabbing this tile's grid Y/X values
    // return at the center origin of this tile
    sf::Vector3f returnPoint = sf::Vector3f(
      tilePos.x * tileSize.x / 2 + tileSize.x / 4.0f,
      tilePos.y * tileSize.y + tileSize.y / 2.0f,
      netWarpTilePos.z
    );

    Poco::UInt16 port = remoteAddress.port();

    auto teleportToCyberworld = [=] {
      getController().push<segue<BlackWashFade>::to<Overworld::OnlineArea>>(host, port, "", maxPayloadSize);
    };

    this->TeleportUponReturn(returnPoint);
    playerController.ReleaseActor();

    auto& command = teleportController.TeleportOut(playerActor);
    command.onFinish.Slot(teleportToCyberworld);
  }
}

void Overworld::Homepage::onEnd()
{
  if (packetProcessor) {
    Net().DropProcessor(packetProcessor);
  }
}

void Overworld::Homepage::OnInteract(Interaction type) {
  if(type != Interaction::action) {
    return;
  }

  ActorPtr player = GetPlayer();
  sf::Vector2f targetPos = player->PositionInFrontOf();
  sf::Vector2f targetOffset = targetPos - player->getPosition();

  for (const ActorPtr& other : GetSpatialMap().GetChunk(targetPos.x, targetPos.y)) {
    if (player == other) continue;

    std::optional<sf::Vector2f> collision = player->CollidesWith(*other, targetOffset);

    if (collision) {
      other->Interact(player, type);
      break;
    }
  }
}
