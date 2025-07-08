#include <Swoosh/ActivityController.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/PixelateBlackWashFade.h>
#include <chrono>

#include "bnNetworkBattleScene.h"
#include "../../bnFadeInState.h"
#include "../../bnElementalDamage.h"
#include "../../bnBlockPackageManager.h"
#include "../../bnPlayerHealthUI.h"

// states 
#include "states/bnNetworkSyncBattleState.h"
#include "../../battlescene/States/bnRewardBattleState.h"
#include "../../battlescene/States/bnTimeFreezeBattleState.h"
#include "../../battlescene/States/bnBattleStartBattleState.h"
#include "../../battlescene/States/bnBattleOverBattleState.h"
#include "../../battlescene/States/bnFadeOutBattleState.h"
#include "../../battlescene/States/bnCombatBattleState.h"
#include "../../battlescene/States/bnCardSelectBattleState.h"
#include "../../battlescene/States/bnCardComboBattleState.h"

// Android only headers
#include "../../Android/bnTouchArea.h"

// modals like card cust and battle reward slide in 12px per frame for 10 frames. 60 frames = 1 sec
// modal slide moves 120px in 1/6th of a second
// Per 1 second that is 6*120px in 6*1/6 of a sec = 720px in 1 sec
#define MODAL_SLIDE_PX_PER_SEC 720.0f

// Combos are counted if more than one enemy is hit within x frames
// The game is clocked to display 60 frames per second
// If x = 20 frames, then we want a combo hit threshold of 20/60 = 0.3 seconds
#define COMBO_HIT_THRESHOLD_SECONDS 20.0f/60.0f

using namespace swoosh::types;
using swoosh::ActivityController;

NetworkBattleScene::NetworkBattleScene(ActivityController& controller, NetworkBattleSceneProps& _props, BattleResultsFunc onEnd) :
  BattleSceneBase(controller, _props.base, onEnd),
  props(std::move(_props)),
  spawnOrder(props.spawnOrder),
  ping(Font::Style::wide),
  frameNumText(Font::Style::wide)
{
  mob = new Mob(props.base.field);

  // Load players in the correct order, then the mob
  Init();

  if (GetLocalPlayer()->GetTeam() == Team::red) {
    LoadBlueTeamMob(*mob);
  }
  else {
    LoadRedTeamMob(*mob);
  }

  packetProcessor = props.packetProcessor;

  if (props.spawnOrder.empty()) {
    Logger::Log(LogLevel::debug, "Spawn Order list was empty! Aborting.");
    this->Quit(FadeOut::black);
  }

  ping.setPosition(480 - (2.f * 16) - 4, 320 - 2.f); // screen upscaled w - (16px*upscale scale) - (2px*upscale)
  ping.SetColor(sf::Color::Red);

  pingIndicator.setTexture(Textures().LoadFromFile("resources/ui/ping.png"));
  pingIndicator.getSprite().setOrigin(sf::Vector2f(16.f, 16.f));
  pingIndicator.setPosition(480, 320);
  pingIndicator.setScale(2.f, 2.f);
  UpdatePingIndicator(frames(0));

  packetProcessor->SetKickCallback([this] {
    this->Quit(FadeOut::black);
  });

  packetProcessor->SetPacketBodyCallback([this](NetPlaySignals header, const Poco::Buffer<char>& buffer) {
    this->ProcessPacketBody(header, buffer);
  });

  // in seconds
  constexpr double battleDuration = 10.0;

  // First, we create all of our scene states
  auto syncState = AddState<NetworkSyncBattleState>(remotePlayer, this);
  auto cardSelect = AddState<CardSelectBattleState>();
  auto combat = AddState<CombatBattleState>(battleDuration);
  auto combo = AddState<CardComboBattleState>(this->GetSelectedCardsUI(), props.base.programAdvance);
  auto forms = AddState<CharacterTransformBattleState>();
  auto battlestart = AddState<BattleStartBattleState>();
  auto battleover = AddState<BattleOverBattleState>();
  auto timeFreeze = AddState<TimeFreezeBattleState>();
  auto fadeout = AddState<FadeOutBattleState>(FadeOut::black); // this state requires arguments

  // We need to respond to new events later, create a resuable pointer to these states
  timeFreezePtr = &timeFreeze.Unwrap();
  combatPtr = &combat.Unwrap();
  syncStatePtr = &syncState.Unwrap();
  cardComboStatePtr = &combo.Unwrap();
  startStatePtr = &battlestart.Unwrap();
  cardStatePtr = &cardSelect.Unwrap();

  for (std::shared_ptr<Player> p : GetAllPlayers()) {
    std::shared_ptr<PlayerSelectedCardsUI> cardUI = p->GetFirstComponent<PlayerSelectedCardsUI>();
    combatPtr->Subscribe(*cardUI);
    timeFreezePtr->Subscribe(*cardUI);

    // Subscribe to player's events
    combatPtr->Subscribe(*p);
    timeFreezePtr->Subscribe(*p);
  }

  // Important! State transitions are added in order of priority!
  syncState.ChangeOnEvent(cardSelect, &NetworkSyncBattleState::IsRemoteConnected);

  // Goto the combo check state if new cards are selected...
  syncState.ChangeOnEvent(combo, &NetworkSyncBattleState::SelectedNewChips);

  // ... else if forms were selected, go directly to forms ....
  syncState.ChangeOnEvent(forms, &NetworkSyncBattleState::HasForm);

  // ... Finally if none of the above, just start the battle
  syncState.ChangeOnEvent(battlestart, &NetworkSyncBattleState::NoConditions);

  // Wait for handshake to complete by going back to the sync state..
  cardSelect.ChangeOnEvent(syncState, &CardSelectBattleState::OKIsPressed);

  // If we reached the combo state, we must also check if form transformation was next
  // or to just start the battle after
  combo.ChangeOnEvent(forms, [cardSelect, combo, this]() mutable {return combo->IsDone() && (cardSelect->HasForm() || remoteState.remoteChangeForm); });
  combo.ChangeOnEvent(battlestart, &CardComboBattleState::IsDone);

  // Forms is the last state before kicking off the battle
  // if we reached this state...
  forms.ChangeOnEvent(combat, HookFormChangeEnd(forms.Unwrap(), cardSelect.Unwrap()));
  forms.ChangeOnEvent(battlestart, &CharacterTransformBattleState::IsFinished);

  battlestart.ChangeOnEvent(combat, &BattleStartBattleState::IsFinished);
  timeFreeze.ChangeOnEvent(combat, &TimeFreezeBattleState::IsOver);

  // combat has multiple state interruptions based on events
  // so we can chain them together
  combat
    .ChangeOnEvent(battleover, HookPlayerWon(combat.Unwrap(), battleover.Unwrap()))
    .ChangeOnEvent(forms     , HookPlayerDecrosses(forms.Unwrap()))
    .ChangeOnEvent(battleover, HookPlayerLost(combat.Unwrap(), battleover.Unwrap()))
    .ChangeOnEvent(cardSelect, HookOnCardSelectEvent())
    .ChangeOnEvent(timeFreeze, &CombatBattleState::HasTimeFreeze);

  battleover.ChangeOnEvent(fadeout, &BattleOverBattleState::IsFinished);

  // share some values between states
  combo->ShareCardList(cardSelect->GetCardPtrList());

  // Some states need to know about card uses
  auto& ui = this->GetSelectedCardsUI();
  combat->Subscribe(ui);
  timeFreeze->Subscribe(ui);

  // pvp cannot pause
  combat->EnablePausing(false);

  // Some states are part of the combat routine and need to respect
  // the combat state's timers
  combat->subcombatStates.push_back(&timeFreeze.Unwrap());

  // this kicks-off the state graph beginning with the intro state
  this->StartStateGraph(syncState);
}

NetworkBattleScene::~NetworkBattleScene()
{
}

void NetworkBattleScene::OnHit(Entity& victim, const Hit::Properties& props)
{
  bool freezeBreak = victim.IsIceFrozen() && ((props.flags & Hit::breaking) == Hit::breaking);
  bool superEffective = victim.IsSuperEffective(props.element) && props.damage > 0;

  if (freezeBreak || superEffective) {
    std::shared_ptr<ElementalDamage> seSymbol = std::make_shared<ElementalDamage>();
    seSymbol->SetLayer(-100);
    seSymbol->SetHeight(victim.GetHeight() + (victim.getLocalBounds().height * 0.5f)); // place it at sprite height
    GetField()->AddEntity(seSymbol, victim.GetTile()->GetX(), victim.GetTile()->GetY());
  }

  std::shared_ptr<Player> player = GetPlayerFromEntityID(victim.GetID());

  if (!player) return;

  if (props.damage > 0) {
    if (props.damage >= 300) {
      player->SetEmotion(Emotion::angry);
      
      std::shared_ptr<PlayerSelectedCardsUI> ui = player->GetFirstComponent<PlayerSelectedCardsUI>();
      
      if (ui) {
        ui->SetMultiplier(2);
      }
    }

    if (player->IsSuperEffective(props.element)) {
      // animate the transformation back to default form
      TrackedFormData& formData = GetPlayerFormData(player);

      if (formData.selectedForm != -1) {
        formData.animationComplete = false;
        formData.selectedForm = -1;
      }

      if (player == GetLocalPlayer()) {
        // Local player needs to update their form selections in the card gui
        cardStatePtr->ResetSelectedForm();
        localPlayerDecross = true;
      }
      else {
        remotePlayerDecross = true;
      }
    }
  }
}

void NetworkBattleScene::onUpdate(double elapsed) {
  if (!IsSceneInFocus()) return;

  SendPingSignal();

  skipFrame = IsRemoteBehind() && this->remotePlayer && !this->remotePlayer->IsDeleted();

  bool skippingUpdate = false;
  if (skipFrame && FrameNumber()-resyncFrameNumber >= frames(5)) {
    SkipFrame();
    skippingUpdate = true;
  }
  else {
    std::vector<InputEvent> events;
    if (combatPtr->IsStateCombat(GetCurrentState())) {
      events = ProcessLocalPlayerInputQueue(5);
    }

    SendFrameData(events, (FrameNumber() + frames(5)).count());
  }


  auto tiles = this->GetField()->FindTiles([](Battle::Tile*) -> bool {return true; });

  std::string skipPart1 = skippingUpdate ? "Skipping over " : "";

  std::string fieldState = skipPart1 + "Battle Frame " + std::to_string(this->FrameNumber().count()) + " tiles: ";

  for (int i = 0; i < tiles.size(); i++) {
    int state = static_cast<int>(tiles[i]->GetState());
    fieldState = fieldState + (state < 10 ? ("0" + std::to_string(state)) : std::to_string(state));
  }



  // Combat started 2f early on log2? 463 and 465

  fieldState = fieldState + " Combat? " + std::to_string(combatPtr->IsStateCombat(GetCurrentState()))
    + " Freeze? " + std::to_string(combatPtr->HasTimeFreeze()) + ", " + std::to_string(timeFreezePtr->summonTick.count()) + ", " + std::to_string(timeFreezePtr->IsOver())
    + " Combo? " + std::to_string(cardComboStatePtr->IsDone())
    + " Start? " + std::to_string(startStatePtr->IsFinished()) + ", " + std::to_string(startStatePtr->timer.elapsed().count())
    + " Custom? " + std::to_string(GetCustomBarProgress());


  Logger::Log(LogLevel::net, fieldState);
  
  if (!remoteInputQueue.empty()) {
    auto frame = remoteInputQueue.begin();

    const uint64_t sceneFrameNumber = FrameNumber().count();
    if (sceneFrameNumber >= frame->frameNumber) {
      if (sceneFrameNumber != frame->frameNumber) {
        // for debugging, this should never appear if the code is working properly
        Logger::Logf(LogLevel::debug, "DESYNC: frames #s were R%i - L%i, ahead by %i", frame->frameNumber, sceneFrameNumber, sceneFrameNumber - frame->frameNumber);
      }

      std::vector<InputEvent>& events = frame->events;
      remoteFrameNumber = frames(frame->frameNumber);

      // Logger::Logf("next remote frame # is %i", remoteFrameNumber);
      

      std::string skipPart = skippingUpdate ? "Skipping over " : "";
      std::string inp = skipPart + "Frame " + std::to_string(frame->frameNumber) + ": Remote inputs: ";
      for (int i = 0; i < events.size(); i++) {
        inp = inp + "" + events[i].name + " " + std::to_string(static_cast<int>(events[i].state));
      }

      Logger::Log(LogLevel::net, inp);

      for (InputEvent& e : events) {

        remotePlayer->InputState().VirtualKeyEvent(e);
      }

      frame = remoteInputQueue.erase(frame);
    }
    else {
      Logger::Log(LogLevel::net, "Skip remote input keys because scene frame number is less than frame number (" + std::to_string(sceneFrameNumber) + " < " + std::to_string(frame->frameNumber) + ")");
    }
  }
  else {
    Logger::Log(LogLevel::net, "Skip remote input keys because queue is empty");
  }

  BattleSceneBase::onUpdate(elapsed);

  frame_time_t elapsed_frames = from_seconds(elapsed);

  if (!syncStatePtr->IsSynchronized()) {
    if (packetProcessor->IsHandshakeAck() && remoteState.remoteHandshake) {
      Logger::Log(LogLevel::net, "Handshake ackknowledged, declare synchronized");
      syncStatePtr->Synchronize();
    }
  }
  else {
    packetTime += elapsed_frames;
  }

  if (skipFrame) return;

  if (remotePlayer && remotePlayer->WillEraseEOF()) {
    UntrackOtherPlayer(remotePlayer);
    remoteState.remoteConnected = false;
    remotePlayer = nullptr;
  }

  if (std::shared_ptr<Player> player = GetLocalPlayer()) {
    BattleResultsObj().finalEmotion = player->GetEmotion();
  }
}

void NetworkBattleScene::onDraw(sf::RenderTexture& surface) {
  BattleSceneBase::onDraw(surface);

  const double lag = GetAvgLatency();

  // draw network ping
  ping.SetString(std::to_string(lag).substr(0, 5));

  //the bounds has been changed after changing the text
  sf::FloatRect bounds = ping.GetLocalBounds();
  ping.setOrigin(bounds.width, bounds.height);

  // draw ping
  surface.draw(ping);

  frameNumText.SetString("F" + std::to_string(FrameNumber().count()));

  bounds = frameNumText.GetLocalBounds();
  frameNumText.setOrigin(bounds.width, bounds.height);

  frameNumText.SetColor(sf::Color::Cyan);
  frameNumText.setPosition(480 - (2.f * 128) - 4, 320 - 2.f);
  surface.draw(frameNumText);

  frameNumText.SetString("F" + std::to_string(remoteFrameNumber.count()));

  bounds = frameNumText.GetLocalBounds();
  frameNumText.setOrigin(bounds.width, bounds.height);

  frameNumText.SetColor(sf::Color::Magenta);
  frameNumText.setPosition(480 - (2.f * 64) - 4, 320 - 2.f);
  surface.draw(frameNumText);

  // convert from ms to seconds to discrete frame count...
  frame_time_t lagTime = from_milliseconds(lag);

  // use ack to determine connectivity here
  UpdatePingIndicator(lagTime);

  //draw
  surface.draw(pingIndicator);
}

void NetworkBattleScene::onExit()
{
}

void NetworkBattleScene::onEnter()
{
}

void NetworkBattleScene::onStart()
{
  BattleSceneBase::onStart();
  packetProcessor->EnableKickForSilence(true);
}

void NetworkBattleScene::onResume()
{
}

void NetworkBattleScene::onEnd()
{
  BattleSceneBase::onEnd();
  getController().SetSubtitle("");
}

const NetPlayFlags& NetworkBattleScene::GetRemoteStateFlags()
{
  return remoteState; 
}

const double NetworkBattleScene::GetAvgLatency() const
{
  return packetProcessor->GetAvgLatency();
}

bool NetworkBattleScene::IsRemoteBehind()
{
  return FrameNumber() > this->maxRemoteFrameNumber;
}

void NetworkBattleScene::Init()
{
  BlockPackagePartitioner& partition = getController().BlockPackagePartitioner();

  size_t idx = 0;
  for (auto& [blocks, p, x, y] : spawnOrder) {
    if (p == GetLocalPlayer()) {
      std::string title = "Player #" + std::to_string(idx+1);
      SpawnLocalPlayer(x, y);
      getController().SetSubtitle(title);
    }
    else {
      // Spawn and subscribe to remote player's events
      SpawnRemotePlayer(p, x, y);
    }

    // Run block programs on the remote player now that they are spawned
    for (const PackageAddress& addr: blocks) {
      BlockPackageManager& blockPackages = partition.GetPartition(addr.namespaceId);
      if (!blockPackages.HasPackage(addr.packageId)) continue;

      BlockMeta& blockMeta = blockPackages.FindPackageByID(addr.packageId);
      blockMeta.mutator(*p);
    }

    idx++;
  }

  std::shared_ptr<MobHealthUI> ui = remotePlayer->GetFirstComponent<MobHealthUI>();
  
  if (ui) {
    ui->SetManualMode(true);
    ui->SetHP(remotePlayer->GetHealth());
  }

  GetCardSelectWidget().PreventRetreat();
  GetCardSelectWidget().SetSpeaker(props.mug, props.anim);
  GetEmotionWindow().SetTexture(props.emotion);
}

void NetworkBattleScene::SendHandshakeSignal()
{
  Logger::Log(LogLevel::net, "Sending handshake signal");
  /**
  To begin the round, we need to supply the following information to our opponent:
    1. Our selected form
    2. Our card list size
    3. Our card list items

  We need to also measure latency to synchronize client animation with
  remote animations (see: combos and forms)
  */

  int form = GetPlayerFormData(GetLocalPlayer()).selectedForm;
  unsigned thisFrame = FrameNumber().count();

  if (thisFrame > 0) {
    thisFrame = thisFrame - 1u;
  }

  size_t len = prefilteredCardSelection.size();

  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals signalType{ NetPlaySignals::handshake };
  buffer.append((char*)&signalType, sizeof(NetPlaySignals));
  buffer.append((char*)&thisFrame, sizeof(unsigned));
  buffer.append((char*)&form, sizeof(int));
  buffer.append((char*)&len, sizeof(size_t));

  CardPackagePartitioner& partitioner = getController().CardPackagePartitioner();
  CardPackageManager& localPackages = partitioner.GetPartition(Game::LocalPartition);
  CardPackageManager& remotePackages = partitioner.GetPartition(Game::RemotePartition);
  for (std::string& id : prefilteredCardSelection) {
    if (localPackages.HasPackage(id)) {
      id = remotePackages.WithNamespace(id);
    }
    else {
      id = "";
    }

    size_t len = id.size();
    buffer.append((char*)&len, sizeof(size_t));
    buffer.append(id.c_str(), len);
  }

  auto [_, id] = packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
  packetProcessor->UpdateHandshakeID(id);
}

void NetworkBattleScene::SendFrameData(std::vector<InputEvent>& events, unsigned int frameNumber)
{
  std::string inp = "Frame " + std::to_string(frameNumber) + ": Sending inputs: ";
  for (int i = events.size() - 1; i >= 0; i--) {
    inp = inp + events[i].name + " " + std::to_string(static_cast<int>(events[i].state));
  }

  Logger::Log(LogLevel::net, inp);
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals signalType{ NetPlaySignals::frame_data };
  buffer.append((char*)&signalType, sizeof(NetPlaySignals));
  buffer.append((char*)&frameNumber, sizeof(unsigned int));

  // Send our hp
  int hp = 0;
  if (auto player = GetLocalPlayer()) {
    hp = player->GetHealth();
  }

  buffer.append((char*)&hp, sizeof(int));

  // send the input keys
  size_t list_len = events.size();
  buffer.append((char*)&list_len, sizeof(size_t));

  while (list_len > 0) {
    size_t len = events[list_len-1].name.size();
    buffer.append((char*)&len, sizeof(size_t));
    buffer.append(events[list_len-1].name.c_str(), len);
    buffer.append((char*)&events[list_len-1].state, sizeof(InputState));
    list_len--;
  }

  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
  packetTime = frames(0);
  events.clear();
}

void NetworkBattleScene::SendPingSignal()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::ping };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::RecieveHandshakeSignal(const Poco::Buffer<char>& buffer)
{
  if (!remoteState.remoteConnected) return;

  // clear remote and local input queues
  remoteInputQueue.clear();
  FlushLocalPlayerInputQueue();

  std::vector<std::string> remoteUUIDs;
  int remoteForm{ -1 };
  size_t cardLen{};
  size_t read{};

  std::memcpy(&remoteFrameNumber, buffer.begin(), sizeof(unsigned));
  maxRemoteFrameNumber = remoteFrameNumber;
  read += sizeof(unsigned);

  std::memcpy(&remoteForm, buffer.begin() + read, sizeof(int));
  read += sizeof(int);

  std::memcpy(&cardLen, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

  Logger::Logf(LogLevel::debug, "Recieved remote handshake. Remote sent %i cards.", cardLen);
  while (cardLen > 0) {
    std::string uuid;
    size_t len{};
    std::memcpy(&len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);
    uuid.resize(len);
    std::memcpy(uuid.data(), buffer.begin() + read, len);
    read += len;
    remoteUUIDs.push_back(uuid);
    Logger::Logf(LogLevel::debug, "Remote Card: %s", uuid.c_str());
    cardLen--;
  }

  // Now that we have the remote's form and cards,
  // populate the net play remote state with this information
  // and kick off the battle sequence

  //remoteState.remoteFormSelect = remoteForm;
  remoteState.remoteChangeForm = remoteState.remoteFormSelect != remoteForm;
  remoteState.remoteFormSelect = remoteForm;

  TrackedFormData& formData = GetPlayerFormData(remotePlayer);
  formData.selectedForm = remoteForm;
  formData.animationComplete = !remoteState.remoteChangeForm; // a value of false forces animation to play

  remoteHand.clear();

  size_t handSize = remoteUUIDs.size();
  int len = (int)handSize;

  CardPackagePartitioner& partition = getController().CardPackagePartitioner();
  CardPackageManager& localPackageManager = partition.GetPartition(Game::LocalPartition);
  if (handSize) {
    for (size_t i = 0; i < handSize; i++) {
      Battle::Card card;
      std::string id = remoteUUIDs[i];
      PackageAddress addr = PackageAddress::FromStr(id).value();

      CardPackageManager& packageManager = partition.GetPartition(addr.namespaceId);

      if (packageManager.HasPackage(addr.packageId)) {
        card = packageManager.FindPackageByID(addr.packageId).GetCardProperties();
        card.props.uuid = packageManager.WithNamespace(card.props.uuid);
      }
      else if(localPackageManager.HasPackage(addr.packageId)) {
        card = localPackageManager.FindPackageByID(addr.packageId).GetCardProperties();
        card.props.uuid = localPackageManager.WithNamespace(card.props.uuid);
      }

      remoteHand.push_back(card);
    }
  }

  // Prepare for simulation
  cardComboStatePtr->Reset();
  double duration{};

  // simulate PA to calculate time required to animate
  while (!cardComboStatePtr->IsDone()) {
    constexpr double step = 1.0 / frame_time_t::frames_per_second;
    cardComboStatePtr->Simulate(step, remotePlayer, remoteHand, false);
    duration += step;
  }

  // Prepare for next PA
  cardComboStatePtr->Reset();

  // Filter support cards
  FilterSupportCards(remotePlayer, remoteHand);

  // Supply the final hand info
  remoteCardActionUsePublisher->LoadCards(remoteHand);
  
  // Convert to microseconds and use this as the round start delay
  roundStartDelay = from_milliseconds((long long)((duration*1000.0) + packetProcessor->GetAvgLatency()));

  // startStatePtr->SetStartupDelay(roundStartDelay);
  startStatePtr->SetStartupDelay(frames(5));

  remoteState.remoteHandshake = true;
}

void NetworkBattleScene::RecieveFrameData(const Poco::Buffer<char>& buffer)
{
  if (!remotePlayer) return;

  std::string name;
  size_t len{}, list_len{};
  size_t read{};

  unsigned int frameNumber{};

  
  std::memcpy(&frameNumber, buffer.begin(), sizeof(unsigned int));
  read += sizeof(unsigned int);

  Logger::Log(LogLevel::net, "Received remote inputs for " + std::to_string(frameNumber));
  maxRemoteFrameNumber = frames(frameNumber);

  int hp{};
  std::memcpy(&hp, buffer.begin() + read, sizeof(int));
  read += sizeof(int);

  std::memcpy(&list_len, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

  std::vector<InputEvent> events;
  while (list_len-- > 0) {
    std::memcpy(&len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);

    name.clear();
    name.resize(len);
    std::memcpy(name.data(), buffer.begin() + read, len);
    read += len;

    InputState state{};
    std::memcpy(&state, buffer.begin() + read, sizeof(InputState));
    read += sizeof(InputState);

    InputEvent event{};
    event.name = name;
    event.state = state;
    events.push_back(event);
  }

  remoteInputQueue.push_back({ frameNumber, events });
  
  if (remotePlayer) {
    std::shared_ptr<MobHealthUI> ui = remotePlayer->GetFirstComponent<MobHealthUI>();
    remotePlayer->SetHealth(hp);

    if (ui) {
      ui->SetHP(hp);
    }

    // HACK: manually delete remote so we see them die when they say they do
    // NOTE: this will be removed when lockstep is PERFECT as it is not needed
    if (hp == 0) {
      GetField()->CharacterDeletePublisher::Broadcast(*remotePlayer.get());
      remotePlayer->Delete();
    }
  }
}

void NetworkBattleScene::SpawnRemotePlayer(std::shared_ptr<Player> newRemotePlayer, int x, int y)
{
  if (remotePlayer) return;

  if (!newRemotePlayer) {
    Logger::Logf(LogLevel::critical, "PVP tried to spawn a nullptr remote player!");
    return;
  }

  remotePlayer = newRemotePlayer;
  remoteState.remoteConnected = true;
  remotePlayer->ManualDelete(); // HACK: prevent local pawn deleting before network player can sync hp....

  // This will add PlayerSelectedCardsUI component to the player
  // NOTE: this calls Init() to get remote player data
  SpawnOtherPlayer(newRemotePlayer, x, y);

  Logger::Logf(LogLevel::debug, "Spawn remote navi finished: %s", remotePlayer->GetName().c_str());

  remoteCardActionUsePublisher = newRemotePlayer->GetFirstComponent<PlayerSelectedCardsUI>();
}

void NetworkBattleScene::OnSelectNewCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards)
{
  // intercept pre-filtered cards to send them over the network
  if (player == GetLocalPlayer()) {
    prefilteredCardSelection.clear();

    for (Battle::Card& card : cards) {
      prefilteredCardSelection.push_back(card.GetUUID());
    }
  }
}

std::function<bool()> NetworkBattleScene::HookPlayerWon(CombatBattleState& combat, BattleOverBattleState& over)
{
  auto lambda = [&combat, &over, this] {
    bool result = GetLocalPlayer()->GetTeam() == Team::red? combat.RedTeamWon() : combat.BlueTeamWon();

    if (result) {
      over.SetIntroText("Enemy Deleted!");
    }

    return result;
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookPlayerLost(CombatBattleState& combat, BattleOverBattleState& over)
{
  auto lambda = [&combat, &over, this] {
    bool result = combat.PlayerDeleted();

    if (result) {
      over.SetIntroText(GetLocalPlayer()->GetName() + " Deleted!");
    }

    return result;
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookPlayerDecrosses(CharacterTransformBattleState& forms)
{
  // special condition: if in combat and should decross, trigger the character transform states
  auto lambda = [this, &forms]() mutable {
    bool changeState = false;

    // If ANY player is decrossing, we need to change state
    for (std::shared_ptr<Player> player : GetAllPlayers()) {
      TrackedFormData& formData = GetPlayerFormData(player);

      bool decross = player->GetHealth() == 0 && (formData.selectedForm != -1);

      // ensure we decross if their HP is zero and they have not yet
      if (decross) {
        formData.selectedForm = -1;
        formData.animationComplete = false;
      }

      // If the anim form data is configured to decross, then we will
      bool myChangeState = (formData.selectedForm == -1 && formData.animationComplete == false);

      // Accumulate booleans, if any one is true, then the whole is true
      changeState = changeState || myChangeState;
    }

    // If we are going back to our base form, skip the animation backdrop step here
    if (changeState) {
      forms.SkipBackdrop();
    }

    // return result
    return changeState;
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookOnCardSelectEvent()
{
  // Lambda event callback that captures and handles network card select screen opening
  auto lambda = [this]() mutable {
    bool remoteRequestedChipSelect = remotePlayer && remotePlayer->InputState().Has(InputEvents::pressed_cust_menu);
    return combatPtr->PlayerRequestCardSelect() || (remoteRequestedChipSelect && this->IsCustGaugeFull());
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookFormChangeEnd(CharacterTransformBattleState& form, CardSelectBattleState& cardSelect)
{
  auto lambda = [&form, &cardSelect, this]() mutable {
    bool localTriggered = (GetLocalPlayer()->GetHealth() == 0 || localPlayerDecross);
    bool remoteTriggered = (remotePlayer->GetHealth() == 0 || remotePlayerDecross);
    bool triggered = form.IsFinished() && (localTriggered || remoteTriggered);

    if (triggered) {
      remotePlayerDecross = false; // reset our decross flag
      localPlayerDecross = false;

      // update the card select gui and state
      // since the state has its own records
      cardSelect.ResetSelectedForm();
    }

    return triggered;
  };

  return lambda;
}


void NetworkBattleScene::ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body)
{
  try {
    switch (header) {
      case NetPlaySignals::handshake:
        RecieveHandshakeSignal(body);
        break;
      case NetPlaySignals::frame_data:
        RecieveFrameData(body);
        break;
    }
  }
  catch (std::exception& e) {
    Logger::Logf(LogLevel::critical, "PVP Network exception: %s", e.what());
    packetProcessor->HandleError();
  }
}

void NetworkBattleScene::UpdatePingIndicator(frame_time_t frames)
{
  unsigned int idx{};
  const unsigned int count = frames.count();

  if (count >= 20) {
    // 1st frame is red - no connection for a significant fraction of the frame
    // innevitable desync here
    idx = 1;
  }
  else if (count >= 5) {
    // 2nd frame is weak - not strong and prone to desync
    idx = 2;
  }
  else if (count >= 3) {
    // 3rd frame is average - ok but not best
    idx = 3; 
  }
  else  {
    // 4th frame is excellent or zero latency 
    idx = 4;
  }

  pingIndicator.setTextureRect(sf::IntRect((idx-1u)*16, 0, 16, 16));
}