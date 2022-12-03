#include <cmath>
#include <Swoosh/ActivityController.h>
#include <Swoosh/Ease.h>
#include <Swoosh/Game.h>
#include <SFML/Window/Clipboard.hpp>
#include <Segues/PushIn.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/VerticalSlice.h>

#include "bnMatchMakingScene.h"
#include "bnDownloadScene.h"
#include "../bnGridBackground.h"
#include "../bnAudioResourceManager.h"
#include "../bnSecretBackground.h"
#include "../bnMessage.h"
#include "../bnBlockPackageManager.h"
#include "battlescene/bnNetworkBattleScene.h"

using namespace swoosh::types;

MatchMakingScene::MatchMakingScene(swoosh::ActivityController& controller, const std::string& naviId, std::unique_ptr<CardFolder> _folder, PA& pa) :
  textbox(sf::Vector2f(4, 250)),
  selectedNaviId(naviId),
  folder(std::move(_folder)), pa(pa),
  uiAnim("resources/ui/pvp_widget.animation"),
  text(Font::Style::thick),
  id(Font::Style::thick),
  Scene(controller)
{
  // Sprites
  ui.setTexture(Textures().LoadFromFile("resources/ui/pvp_widget.png"));
  vs.setTexture(Textures().LoadFromFile("resources/ui/vs_text.png"));
  vsFaded.setTexture(Textures().LoadFromFile("resources/ui/vs_text.png"));
  greenBg.setTexture(Textures().LoadFromFile(TexturePaths::FOLDER_VIEW_BG));
  navigator.setTexture(Textures().LoadFromFile(TexturePaths::MUG_NAVIGATOR));

  float w = static_cast<float>(controller.getVirtualWindowSize().x);
  float h = static_cast<float>(controller.getVirtualWindowSize().y);
  vs.setPosition(w / 2.f, h / 2.f);
  swoosh::game::setOrigin(vs.getSprite(), 0.5, 0.5);

  // hide this until it is ready
  vs.setScale(0.f, 0.f);

  // this is the fadeout VS effect
  swoosh::game::setOrigin(vsFaded.getSprite(), 0.5, 0.5);
  vsFaded.setPosition(w / 2.f, h / 2.f);
  vsFaded.setScale(0.f, 0.f);

  greenBg.setScale(2.f, 2.f);

  this->gridBG = new GridBackground();
  gridBG->SetColor(sf::Color(0)); // hide until it is ready

  auto& playerPkg = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition).FindPackageByID(selectedNaviId);
  clientPreview.setTexture(playerPkg.GetPreviewTexture());
  clientPreview.setScale(2.f, 2.f);
  clientPreview.setOrigin(clientPreview.getLocalBounds().width, clientPreview.getLocalBounds().height);

  // flip the remote player
  remotePreview.setScale(-2.f, 2.f);

  // text / font
  text.setPosition(45.f, 4.f);

  // upscale text
  text.setScale(2.f, 2.f);
  id.setScale(2.f, 2.f);

  // load animation files
  uiAnim.Load();

  packetProcessor = std::make_shared<MatchMaking::PacketProcessor>();

  setView(sf::Vector2u(480, 320));
}

MatchMakingScene::~MatchMakingScene() {
  delete gridBG;
}

void MatchMakingScene::HandleInfoMode()
{
  textbox.ClearAllMessages();
  Message* help = new Message("CTRL + C to copy your IP address");
  textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
  textbox.Open();
  textbox.CompleteCurrentBlock();
}

void MatchMakingScene::HandleJoinMode()
{
  textbox.ClearAllMessages();
  Message* help = new Message("Paste your opponent's IP address");
  textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
  textbox.Open();
  textbox.CompleteCurrentBlock();
}

void MatchMakingScene::HandleReady()
{
  textbox.ClearAllMessages();
  Message* help = new Message("Waiting for " + theirIP);
  textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
  textbox.Open();
  textbox.CompleteCurrentBlock();
}

void MatchMakingScene::HandleCancel()
{
  textbox.ClearAllMessages();
  Message* help = new Message("Cancelled...");
  textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
  textbox.Open();
  textbox.CompleteCurrentBlock();
  Audio().Play(AudioType::CHIP_ERROR);
}

void MatchMakingScene::HandleGetIPFailure()
{
  textbox.ClearAllMessages();
  Message* help = new Message("Error obtaining your IP");
  textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
  textbox.Open([=]() {
    Audio().Play(AudioType::CHIP_ERROR);
  });
  textbox.CompleteCurrentBlock();
}

void MatchMakingScene::HandleCopyEvent()
{
  std::string value = Input().GetClipboard();

  if (value != myIP) {
    Message* help = nullptr;
    if (myIP.empty()) {
      Audio().Play(AudioType::CHIP_ERROR);
      help = new Message("IP addr unavailable");
    }
    else {
      Input().SetClipboard(myIP);
      Audio().Play(AudioType::NEW_GAME);
      help = new Message("Copied!");
    }

    textbox.ClearAllMessages();
    textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
    textbox.CompleteCurrentBlock();
  }
}

void MatchMakingScene::HandlePasteEvent()
{
  std::string value = Input().GetClipboard();

  // assume true to avoid a series if-else scenarios
  bool hasError = true;

  if (value != theirIP) {
    Message* help = nullptr;

    if (IsValidIPv4(value)) {
      packetProcessor->SetNewRemote(value, Net().GetMaxPayloadSize());

      if (packetProcessor->RemoteAddrIsValid()) {
        theirIP = value;
        Audio().Play(AudioType::COUNTER_BONUS);
        help = new Message("Pasted! Press start to connect.");

        // If everything goes well, we have no error with the address...
        hasError = false;
      }
    }

    if(hasError) {
      Audio().Play(AudioType::CHIP_ERROR);
      help = new Message("Bad IP");
    }

    textbox.ClearAllMessages();
    textbox.EnqueMessage(navigator.getSprite(), "resources/ui/navigator.animation", help);
    textbox.CompleteCurrentBlock();
  }
}

void MatchMakingScene::ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body)
{
  switch (header) {
  case NetPlaySignals::matchmaking_handshake:
    Logger::Log(LogLevel::info, "Received netplay handshake signal");
    RecieveHandshakeSignal();
    break;
  case NetPlaySignals::matchmaking_request:
    Logger::Log(LogLevel::info, "Received netplay connect signal");
    RecieveConnectSignal(body);
    break;
  default:
    Logger::Log(LogLevel::info, "Received unsupported signal in netplay");
  }
}

void MatchMakingScene::SendConnectSignal()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::matchmaking_request };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void MatchMakingScene::SendHandshakeSignal()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::matchmaking_handshake };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void MatchMakingScene::RecieveConnectSignal(const Poco::Buffer<char>& buffer)
{
  if (!clientIsReady || remoteIsReady) return;
  remoteIsReady = true;
  SendHandshakeSignal();
}

void MatchMakingScene::RecieveHandshakeSignal()
{
  // only acknowledge handshakes if you have recieved the cnnect signal
  if (!remoteIsReady || !clientIsReady) return;

  packetProcessor->SetPacketBodyCallback(nullptr); // remove the packet callback to store packets for the next scene
  this->handshakeComplete = true;
}

void MatchMakingScene::DrawIDInputWidget(IRenderer& renderer)
{
  uiAnim.SetAnimation("ID_START");
  uiAnim.SetFrame(0, ui.getSprite());
  ui.setPosition(100, 40);
  // TODO: remove Clone()
  renderer.submit(&ui);

  if (infoMode) {
    id.SetString(sf::String(myIP));
  }
  else {
    id.SetString(sf::String(theirIP));
  }
  id.setPosition(145, 48);

  // restrict the widget from collapsing at text length 0
  float widgetWidth = std::fmax(id.GetLocalBounds().width*2.f +10.f, 50.f);

  uiAnim.SetAnimation("ID_MID");
  uiAnim.Update(0, ui.getSprite());
  ui.setScale(widgetWidth, 1.0f);
  ui.setPosition(140, 40);

  // TODO: remove Clone()
  renderer.submit(&ui);
  renderer.submit(&id);

  uiAnim.SetAnimation("ID_END");
  uiAnim.Update(0, ui.getSprite());
  ui.setScale(1.f, 1.0f);
  ui.setPosition(140 + widgetWidth, 40);

  // TODO: remove Clone()
  renderer.submit(&ui);
}

void MatchMakingScene::DrawCopyPasteWidget(IRenderer& renderer)
{
  std::string state = "ENABLED";
  std::string clipboard = Input().GetClipboard();

  if (!infoMode && (clipboard.empty() || theirIP == clipboard)) {
    state = "DISABLED";
  }
  else if (!infoMode && theirIP == clipboard) {
    // also disable if already entered
    state = "DISABLED";
  }

  if (infoMode && (myIP.empty() || myIP == clipboard)) {
    state = "DISABLED";
  }

  std::string prefix = "BTN_" + state +"_";
  std::string start = prefix + "START";
  std::string mid = prefix + "MID";
  std::string end = prefix + "END";

  std::string icon = "ICO_" + state + "_";

  uiAnim.SetAnimation(start);
  uiAnim.SetFrame(0, ui.getSprite());
  ui.setPosition(100, 90);

  // TODO: remove Clone()
  renderer.submit(&ui);

  Text widgetText(Font::Style::thick);
  widgetText.setScale(2.f, 2.f);

  if (infoMode) {
    widgetText.SetString("Copy");
    icon += "COPY";
  }
  else {
    widgetText.SetString("Paste");
    icon += "PASTE";
  }

  uiAnim.SetAnimation(icon);
  uiAnim.SetFrame(0, ui.getSprite());
  ui.setPosition(102, 92); // offset by 2 pixels to fit inside the frame
  renderer.submit(&ui);

  widgetText.setPosition(145, 94);

  float widgetWidth = widgetText.GetLocalBounds().width*2.f + 10.f;

  uiAnim.SetAnimation(mid);
  uiAnim.Update(0, ui.getSprite());
  ui.setScale(widgetWidth, 1.0f);
  ui.setPosition(140, 90);

  // TODO: remove Clone()'s
  renderer.submit(&ui);
  renderer.submit(Clone(widgetText));

  uiAnim.SetAnimation(end);
  uiAnim.Update(0, ui.getSprite());
  ui.setScale(1.f, 1.0f);
  ui.setPosition(140 + widgetWidth, 90);

  // TODO: remove Clone()
  renderer.submit(&ui);
}

const bool MatchMakingScene::IsValidIPv4(const std::string& ip) const {
  std::string_view home = "127.0.0.1";
  std::string_view local = "localhost";
  std::string_view colon = ":";

  if (ip.find(home) != std::string::npos || ip.find(local) != std::string::npos) {
    size_t pos = ip.find(colon);
    std::string port = ip.substr(pos);

    // Do not connect to ourselves...
    // Note: bad things happen but really shouldn't here, why?
    //       you can connect fine in overworld...
    if (atoi(port.c_str()) == Net().GetSocket().address().port()) {
      return false;
    }
  }

  /*Poco::Net::IPAddress temp;
  return Poco::Net::IPAddress::tryParse(ip, temp);*/
  return true; // for debugging now...
}

void MatchMakingScene::Reset()
{
  remoteIsReady = false;
  clientIsReady = false;
  isInFlashyVSIntro = false;
  isInBattleStartup = false;
  handshakeComplete = false;
  hasProcessedCards = false;
  canProceedToBattle = false;
  playVS = true;
  sequenceTimer = 0.0; // in seconds
  flashCooldown = 0;
  remoteNaviPackage = PackageAddress{};

  this->isScreenReady = true;

  // hide this until it is ready
  vs.setScale(0.f, 0.f);

  // minor optimzation
  if (myIP.empty()) {
    myIP = Net().GetPublicIP();
  }

  if (myIP.empty()) {
    // there was a problem
    HandleGetIPFailure();
  }
  else {
    if (theirIP.empty()) {
      Logger::Logf(LogLevel::info, "My IP came back as %s", myIP.c_str());
      // start on info mode first
      HandleInfoMode();
    }
    else {
      HandleJoinMode();
    }
  }

  greenBg.setColor(sf::Color::White);
  gridBG->SetColor(sf::Color(0)); // hide until it is ready

  clientPreview.setPosition(0, 0);
  remotePreview.setPosition(0, 0);

  // don't use the last scene to process packets, this scene should process them now
  using namespace std::placeholders;
  packetProcessor->SetPacketBodyCallback(std::bind(&MatchMakingScene::ProcessPacketBody, this, _1, _2));
  packetProcessor->SetKickCallback([] {});
  packetProcessor->SetNewRemote(theirIP, Net().GetMaxPayloadSize());
  Net().DropProcessor(packetProcessor);
}

void MatchMakingScene::onStart() {
  Reset();
}

void MatchMakingScene::onResume() {
  isScreenReady = true;

  Logger::Logf(LogLevel::info, "remote navi ID is: %s", remoteNaviPackage.packageId.c_str());
  Logger::Logf(LogLevel::info, "canProceedToBattle: %d", canProceedToBattle? 1 : 0);

  switch (returningFrom) {
  case ReturningScene::BattleScene:
    Reset();
    break;
  case ReturningScene::DownloadScene:
    if (!canProceedToBattle) {
      // If this condition is false, we could not download assets we needed
      // Reset for next match attempt
      Reset();
    }
    else if(remoteNaviPackage.HasID()) {
      PlayerMeta& playerPkg = getController().PlayerPackagePartitioner().FindPackageByAddress(remoteNaviPackage);
      this->remotePreview.setTexture(playerPkg.GetPreviewTexture());
      auto height = remotePreview.getSprite().getLocalBounds().height;
      remotePreview.setOrigin(sf::Vector2f(0, height));
    }
    break;
  case ReturningScene::Null:
    Logger::Log(LogLevel::critical, "`returningFrom` has not been set for the scene you returned from");
  }

  returningFrom = ReturningScene::Null;
}

void MatchMakingScene::onExit() {
}

void MatchMakingScene::onEnd()
{
  Net().DropProcessor(packetProcessor);
}

void MatchMakingScene::onUpdate(double elapsed) {
  gridBG->Update(double(elapsed));
  textbox.Update(double(elapsed));

  if (!isScreenReady || closing || returningFrom != ReturningScene::Null) return; // don't update our scene if not fully in view from segue

  bool systemEvent = Input().HasSystemCopyEvent() || Input().HasSystemPasteEvent();

  if (handshakeComplete && hasProcessedCards && !isInFlashyVSIntro) {
    isInFlashyVSIntro = true;
    Audio().StopStream();
  }
  else if (handshakeComplete && !hasProcessedCards) {
    hasProcessedCards = true;

    std::vector<PackageHash> cardHashes, selectedNaviBlocks;

    BlockPackageManager& blockPackages = getController().BlockPackagePartitioner().GetPartition(Game::LocalPartition);
    CardPackageManager& cardPackages = getController().CardPackagePartitioner().GetPartition(Game::LocalPartition);
    PlayerPackageManager& playerPackages = getController().PlayerPackagePartitioner().GetPartition(Game::LocalPartition);

    GameSession& session = getController().Session();
    for (const PackageAddress& blockAddr : PlayerCustScene::GetInstalledBlocks(selectedNaviId, session)) {
      const std::string& blockId = blockAddr.packageId;
      if (!blockPackages.HasPackage(blockAddr.packageId)) continue;
      const std::string& md5 = blockPackages.FindPackageByID(blockId).GetPackageFingerprint();
      selectedNaviBlocks.push_back({ blockId, md5 });
    }

    auto copy = folder->Clone();
    auto next = copy->Next();

    while (next) {
      const std::string& cardId = next->GetUUID();
      next = copy->Next();

      if (!cardPackages.HasPackage(cardId)) continue;
      const std::string& md5 = cardPackages.FindPackageByID(cardId).GetPackageFingerprint();
      cardHashes.push_back({ cardId, md5 });
    }

    PackageHash playerHash;
    playerHash.packageId = selectedNaviId;
    playerHash.md5 = playerPackages.FindPackageByID(selectedNaviId).GetPackageFingerprint();

    this->remotePlayerBlocks.clear();

    DownloadSceneProps props = {
      cardHashes,
      selectedNaviBlocks,
      playerHash,
      packetProcessor->GetRemoteAddr(),
      packetProcessor->GetProxy(),
      screen,
      canProceedToBattle,
      pvpCoinFlip,
      remoteNaviPackage,
      remotePlayerBlocks
    };

    using effect = swoosh::types::segue<VerticalSlice, milliseconds<500>>;
    getController().push<effect::to<DownloadScene>>(props);
    returningFrom = ReturningScene::DownloadScene;
  }
  else if (clientIsReady && !remoteIsReady) {
    if (Input().Has(InputEvents::pressed_cancel) && !systemEvent) {
      clientIsReady = false;
      Net().DropProcessor(packetProcessor);
      HandleCancel();
      return;
    }
  }
  else if (isInFlashyVSIntro && !isInBattleStartup) {
    // use swoosh deltas on the timer to create a sequence
    this->sequenceTimer += elapsed;

    double delta = swoosh::ease::linear(sequenceTimer, 0.5, 1.0);
    sf::Uint8 amt = sf::Uint8(255 * (1 - delta));
    greenBg.setColor(sf::Color(amt,amt,amt,255));

    // Refresh mob graphic origin every frame as it may change
    auto size = getController().getVirtualWindowSize();

    if (delta == 1) {
      delta = swoosh::ease::linear(sequenceTimer - 0.5, 0.5, 1.0);
      float x = float(delta) * 160.0f;

      clientPreview.setPosition(float(delta * size.x) * 0.25f, float(size.y));
      clientPreview.setOrigin(float(clientPreview.getTextureRect().width) * 0.5f, float(clientPreview.getTextureRect().height));

      remotePreview.setPosition(size.x - float(delta * size.x * 0.25f), float(size.y));
      remotePreview.setOrigin(float(remotePreview.getTextureRect().width) * 0.5f, float(remotePreview.getTextureRect().height));

      delta = swoosh::ease::bezierPopIn(sequenceTimer - 0.5, 0.5);
      float scale = std::fabs(float(1.0 - delta) * 20.0f) + 1.f;
      vs.setScale(scale, scale);

      if (delta == 1) {
        // do once
        if (playVS) {
          playVS = false;
          Audio().Play(AudioType::CUSTOM_BAR_FULL, AudioPriority::highest);
          flashCooldown = 20.0 / 60.0;
        }

        flashCooldown -= elapsed;

        // make bg appear
        gridBG->SetColor(sf::Color(255, 255, 255, 255));

        if (flashCooldown > 20) {
          delta = swoosh::ease::linear(sequenceTimer - 1.0, 2.0, 1.0);
          float scale = static_cast<float>(delta) + 1.f;
          vsFaded.setScale(scale, scale);
          amt = sf::Uint8(150 * (1 - delta));
          vsFaded.setColor(sf::Color(255, 255, 255, amt));
        }
      }
    }

    if (this->sequenceTimer >= 3) {
      // Shuffle our folder
      auto copy = folder->Clone();
      copy->Shuffle();

      // Queue screen transition to Battle Scene with a white fade effect
      // just like the game
      using effect = segue<WhiteWashFade>;

      // Play the pre battle sound
      Audio().Play(AudioType::PRE_BATTLE, AudioPriority::high);

      // Stop music and go to battle screen
      Audio().StopStream();

      // Configure the session
      PlayerPackagePartitioner& playerPartitioner = getController().PlayerPackagePartitioner();
      BlockPackagePartitioner& blockPartitioner = getController().BlockPackagePartitioner();

      PlayerMeta& meta = playerPartitioner.FindPackageByAddress({ Game::LocalPartition, selectedNaviId });
      const std::filesystem::path& image = meta.GetMugshotTexturePath();
      const std::filesystem::path& mugshotAnim = meta.GetMugshotAnimationPath();
      const std::filesystem::path& emotionsTexture = meta.GetEmotionsTexturePath();
      std::shared_ptr<sf::Texture> mugshot = Textures().LoadFromFile(image);
      std::shared_ptr<sf::Texture> emotions = Textures().LoadFromFile(emotionsTexture);
      std::shared_ptr<Player> player = std::shared_ptr<Player>(meta.GetData());

      GameSession& session = getController().Session();
      std::vector<PackageAddress> localPlayerBlocks = PlayerCustScene::GetInstalledBlocks(selectedNaviId, session);

      auto& remoteMeta = playerPartitioner.FindPackageByAddress(remoteNaviPackage);
      auto remotePlayer = std::shared_ptr<Player>(remoteMeta.GetData());

      std::vector<NetworkPlayerSpawnData> spawnOrder;
      spawnOrder.push_back({ localPlayerBlocks, player });
      spawnOrder.push_back({ remotePlayerBlocks, remotePlayer });

      // Make player who can go first the priority in the list
      std::iter_swap(spawnOrder.begin(), spawnOrder.begin() + this->pvpCoinFlip);

      // red team goes first
      spawnOrder[0].x = 2;
      spawnOrder[0].y = 2;

      // blue team goes second
      spawnOrder[1].x = 5;
      spawnOrder[1].y = 2;

      NetworkBattleSceneProps props = {
        { player, pa, std::move(copy), std::make_shared<Field>(6, 3), std::make_shared<SecretBackground>() },
        sf::Sprite(*mugshot),
        mugshotAnim,
        emotions,
        packetProcessor->GetProxy(),
        spawnOrder
      };

      getController().push<effect::to<NetworkBattleScene>>(props);
      returningFrom = ReturningScene::BattleScene;
    }
  } else {
    // TODO use states/switches this is getting out of hand...
    if (Input().Has(InputEvents::pressed_cancel) && !systemEvent) {
      closing = true;
      Audio().Play(AudioType::CHIP_CANCEL);
      using effect = segue<PushIn<direction::up>, milliseconds<500>>;
      getController().pop<effect>();
    }
    else if (Input().HasSystemCopyEvent() && infoMode) {
      HandleCopyEvent();
    }
    else if (Input().HasSystemPasteEvent() && !infoMode) {
      HandlePasteEvent();
    }
    else if (Input().Has(InputEvents::pressed_shoulder_left) && infoMode && !systemEvent) {
      infoMode = false;
      Audio().Play(AudioType::CHIP_DESC_CLOSE);
      HandleJoinMode();
    }
    else if (Input().Has(InputEvents::pressed_shoulder_right) && !infoMode && !systemEvent) {
      infoMode = true;
      Audio().Play(AudioType::CHIP_DESC_CLOSE);
      HandleInfoMode();
    }
    else if (Input().Has(InputEvents::pressed_confirm) && !infoMode && packetProcessor->RemoteAddrIsValid() && !systemEvent) {
      Net().AddHandler(packetProcessor->GetRemoteAddr(), packetProcessor);

      this->clientIsReady = true;
      SendConnectSignal();
      HandleReady();
      Audio().Play(AudioType::CHIP_CHOOSE);
    }
  }
}

void MatchMakingScene::onLeave()
{
  isScreenReady = false;
}

void MatchMakingScene::onEnter()
{
  if (returningFrom == ReturningScene::BattleScene) {
    Reset();
  }
}

void MatchMakingScene::onDraw(IRenderer& renderer) {
  renderer.submit(&greenBg);
  renderer.submit(gridBG);

  if (!isInFlashyVSIntro) {
    renderer.submit(&textbox);

    if (infoMode && myIP.empty()) {
      id.SetString("ERROR");
    }

    if (infoMode) {
      text.SetString("My Info");

      uiAnim.SetAnimation("L");
      uiAnim.SetFrame(0, ui.getSprite());
      ui.setPosition(5, 4);
    }
    else {
      text.SetString("Remote");

      uiAnim.SetAnimation("R");
      uiAnim.SetFrame(0, ui.getSprite());
      ui.setPosition(130, 4);
    }

    renderer.submit(&text);

    // L/R icons
    // TODO: remove Clone()
    renderer.submit(&ui);

    this->DrawIDInputWidget(renderer);
    this->DrawCopyPasteWidget(renderer);
  }
  else {
    renderer.submit(&clientPreview);
    renderer.submit(&remotePreview);
    renderer.submit(&vs);
    renderer.submit(&vsFaded);
  }

  if (flashCooldown > 0) {
    const sf::Vector2u winSize = getController().getVirtualWindowSize();
    sf::Vector2f size = sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y));
    sf::RectangleShape screen(size);
    screen.setFillColor(sf::Color::White);
    renderer.submit(Clone(screen));
  }
}
