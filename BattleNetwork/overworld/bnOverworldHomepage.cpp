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
        [this](auto status, auto maxPayloadSize) { UpdateServerStatus(status, maxPayloadSize); }
      );

      Net().AddHandler(remoteAddress, packetProcessor);
    }
    catch (Poco::IOException&) {}
  }

  LoadMap(FileUtil::Read("resources/ow/maps/homepage.tmx"));

  auto& map = GetMap();
  sf::Vector3f spawnPos;
  std::optional<sf::Vector3f> statusBotSpawnOptional, destBotSpawnOptional;

  for (auto i = 0; i < map.GetLayerCount(); i++) {
    auto& layer = map.GetLayer(i);
    auto z = (float)i;

    // search for status bot while it hasn't been found
    if (!statusBotSpawnOptional) {
      auto pointOptional = layer.GetShapeObject("Warp Status Bot");

      if (pointOptional) {
        auto& point = pointOptional->get();
        statusBotSpawnOptional = { point.position.x, point.position.y, z };
      }
    }

    // search for destination bot while it hasn't been found
    if (!destBotSpawnOptional) {
      auto pointOptional = layer.GetShapeObject("Destination Bot");

      if (pointOptional) {
        auto& point = pointOptional->get();
        destBotSpawnOptional = { point.position.x, point.position.y, z };
      }
    }

    // search for warps
    for (auto& tileObject : layer.GetTileObjects()) {
      switch (tileObject.type) {
      case ObjectType::home_warp: {
        auto warpLayerPos = tileObject.position + map.ScreenToWorld(sf::Vector2f(0, tileObject.size.y / 2.0f));
        spawnPos = { warpLayerPos.x, warpLayerPos.y, z };
        break;
      }
      case ObjectType::server_warp: {
        auto centerPos = tileObject.position + map.ScreenToWorld(sf::Vector2f(0, tileObject.size.y / 2.0f));
        auto tileSize = map.GetTileSize();

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
    auto mrprog = std::make_shared<Overworld::Actor>("Mr. Prog");
    mrprog->LoadAnimations("resources/ow/prog/prog_ow.animation");
    mrprog->setTexture(Textures().LoadFromFile("resources/ow/prog/prog_ow.png"));
    mrprog->Set3DPosition(*statusBotSpawnOptional);
    mrprog->SetSolid(true);
    mrprog->SetCollisionRadius(5);

    // we ensure pointer to mrprog is alive because when we interact,
    // mrprog must've been alive to interact in the first place...
    mrprog->SetInteractCallback([mrprog = mrprog.get(), this](const auto& with, const auto& event) {
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

      auto& menuSystem = GetMenuSystem();
      menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
      menuSystem.EnqueueMessage(message);
    });

    AddActor(mrprog);
  }

  if (destBotSpawnOptional) {
    auto mrprog = std::make_shared<Overworld::Actor>("Mr. Prog");
    mrprog->LoadAnimations("resources/ow/prog/prog_ow.animation");
    mrprog->setTexture(Textures().LoadFromFile("resources/ow/prog/prog_ow.png"));
    mrprog->Set3DPosition(destBotSpawnOptional.value());
    mrprog->SetSolid(true);
    mrprog->SetCollisionRadius(11);

    mrprog->SetInteractCallback([mrprog = mrprog.get(), this](const auto& with, const auto& event) {
      // Face them
      mrprog->Face(*with);

      // Play message
      sf::Sprite face;
      face.setTexture(*Textures().LoadFromFile("resources/ow/prog/prog_mug.png"));

      std::string message = "CHANGE YOUR WARP DESTINATION?";

      auto& menuSystem = GetMenuSystem();
      menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
      menuSystem.EnqueueQuestion(message, [=](bool yes) {
        if (!yes) {
          return;
        }

        auto& menuSystem = GetMenuSystem();
        menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
        menuSystem.EnqueueTextInput(getController().Session().GetKeyValue("homepage_warp:0"), 128, [=](const std::string& response) {
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

              auto addrList = Poco::Net::DNS::hostByName(dest).addresses();

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
              [this](auto status, auto maxPayloadSize) { UpdateServerStatus(status, maxPayloadSize); }
            );
            Net().AddHandler(remoteAddress, packetProcessor);
            EnableNetWarps(false);

            auto& menuSystem = GetMenuSystem();
            menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
            menuSystem.EnqueueMessage("CHANGED TO " + response + "!");
            getController().Session().SetKeyValue("homepage_warp:0", response);
            getController().Session().SaveSession("profile.bin");
          }
          catch (...) {
            auto& menuSystem = GetMenuSystem();
            menuSystem.SetNextSpeaker(face, "resources/ow/prog/prog_mug.animation");
            menuSystem.EnqueueMessage("SORRY I COULDN'T SET IT TO " + response + "!");
          }
        });
      });
    });

    AddActor(mrprog);
  }

  // spawn in the player
  auto player = GetPlayer();
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
  auto& map = GetMap();

  auto netWarpOptional = map.GetLayer(0).GetTileObject(netWarpObjectId);

  if (!netWarpOptional) {
    return;
  }

  auto& netWarp = netWarpOptional->get();
  auto tileset = map.GetTileset("warp");

  netWarp.tile.gid = tileset->firstGid + (enabled ? 1 : 0);
}

void Overworld::Homepage::onUpdate(double elapsed)
{
  // Update our logic
  auto& map = GetMap();
  auto& window = getController().getWindow();
  auto mousei = sf::Mouse::getPosition(window);
  const auto& [row, col] = PixelToRowCol(mousei, window);
  sf::Vector2f click = { (float)col * map.GetTileSize().x, (float)row * map.GetTileSize().y };

  auto& worldTransform = GetWorldTransform();

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
    auto mugshot = Textures().LoadFromFile(meta.GetMugshotTexturePath());

    auto& menuSystem = GetMenuSystem();
    menuSystem.SetNextSpeaker(sf::Sprite(*mugshot), meta.GetMugshotAnimationPath());
    menuSystem.EnqueueMessage("This is your homepage.");
    menuSystem.EnqueueMessage("You can edit it anyway you like!");

    GetPlayer()->Face(Direction::down_right);
  }
}

void Overworld::Homepage::onDraw(sf::RenderTexture& surface)
{
  SceneBase::onDraw(surface);
  Text nameText{ Font::Style::small };
}

void Overworld::Homepage::onStart()
{
  SceneBase::onStart();

  Audio().Stream("resources/loops/loop_overworld.ogg", false);
}

void Overworld::Homepage::onResume()
{
  SceneBase::onResume();
  Audio().Stream("resources/loops/loop_overworld.ogg", false);

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
  auto playerActor = GetPlayer();
  auto playerPos = playerActor->getPosition();

  auto& map = GetMap();
  auto tileSize = sf::Vector2f(map.GetTileSize());

  auto tilePos = sf::Vector3f(
    std::floor(playerPos.x / (tileSize.x / 2)),
    std::floor(playerPos.y / tileSize.y),
    std::floor(playerActor->GetElevation())
  );

  auto& teleportController = GetTeleportController();

  if (netWarpTilePos == tilePos && teleportController.IsComplete() && serverStatus == ServerStatus::online) {
    auto& playerController = GetPlayerController();

    // Calculate the origin by grabbing this tile's grid Y/X values
    // return at the center origin of this tile
    auto returnPoint = sf::Vector3f(
      tilePos.x * tileSize.x / 2 + tileSize.x / 4.0f,
      tilePos.y * tileSize.y + tileSize.y / 2.0f,
      netWarpTilePos.z
    );

    auto port = remoteAddress.port();

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

  auto player = GetPlayer();
  auto targetPos = player->PositionInFrontOf();
  auto targetOffset = targetPos - player->getPosition();

  for (const auto& other : GetSpatialMap().GetChunk(targetPos.x, targetPos.y)) {
    if (player == other) continue;

    auto collision = player->CollidesWith(*other, targetOffset);

    if (collision) {
      other->Interact(player, type);
      break;
    }
  }
}
