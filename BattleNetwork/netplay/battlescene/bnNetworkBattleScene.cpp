#include <Swoosh/ActivityController.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/PixelateBlackWashFade.h>
#include <chrono>

#include "bnNetworkBattleScene.h"
#include "../bnBufferReader.h"
#include "../bnBufferWriter.h"
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
  frameNumText(Font::Style::wide) {
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
  auto connectSyncState = AddState<NetworkSyncBattleState>(this);
  auto cardSyncState = AddState<NetworkSyncBattleState>(this);
  auto comboSyncState = AddState<NetworkSyncBattleState>(this);
  auto cardSelect = AddState<CardSelectBattleState>();
  auto combat = AddState<CombatBattleState>(battleDuration);
  auto combo = AddState<CardComboBattleState>(this->GetSelectedCardsUI(), props.base.programAdvance);
  auto forms = AddState<CharacterTransformBattleState>();
  auto battlestart = AddState<BattleStartBattleState>();
  auto battleover = AddState<BattleOverBattleState>();
  auto timeFreeze = AddState<TimeFreezeBattleState>();
  auto fadeout = AddState<FadeOutBattleState>(FadeOut::black); // this state requires arguments

  // We need to respond to new events later, create a resuable pointer to these states
  auto* connectSyncStatePtr = &connectSyncState.Unwrap();

  /*
    Hack? We aren't desynced on battle start because inputs aren't sent until 
    battle is starting. Entering a different time might not actually be a 
    problem.

    Attempt to fix issue where battle could start and never have sync signals 
    properly handled. Could be because one instance has entered battle and tried to 
    send the sync signal, but the other instance was not in battle yet (running 
    behind) and never got it. To cover for this, consider syncs as already 
    reached.

    By not making sure the opponent has actually started battle, it's possible we 
    softlock by exiting card select before the opponent's game has entered the battlescene,
    unless packets will get resent. It appears that both games will hit lockstep by 
    frame 5, though, so this doesn't seem likely.

    If inputs are read during card select in the future (e.g. to sync the frames 
    that custom buttons are pressed), this may desync.
  */ 
  connectSyncStatePtr->MarkRemoteSyncRequested();
  connectSyncStatePtr->MarkSyncRequested();
  auto* cardSyncStatePtr = &cardSyncState.Unwrap();
  auto* comboSyncStatePtr = &comboSyncState.Unwrap();
  syncStates = { connectSyncStatePtr, cardSyncStatePtr, comboSyncStatePtr };
  cardStatePtr = &cardSelect.Unwrap();
  cardComboStatePtr = &combo.Unwrap();
  startStatePtr = &battlestart.Unwrap();
  timeFreezePtr = &timeFreeze.Unwrap();
  combatPtr = &combat.Unwrap();

  for (std::shared_ptr<Player> p : GetAllPlayers()) {
    std::shared_ptr<PlayerSelectedCardsUI> cardUI = p->GetFirstComponent<PlayerSelectedCardsUI>();
    combatPtr->Subscribe(*cardUI);
    timeFreezePtr->Subscribe(*cardUI);

    // Subscribe to player's events
    combatPtr->Subscribe(*p);
    timeFreezePtr->Subscribe(*p);
  }

  // Important! State transitions are added in order of priority!

  // enter card select after the other player joins
  connectSyncState.ChangeOnEvent(cardSelect, &NetworkSyncBattleState::IsReady);

  // Go to the combo state if new cards are selected, or wait at the comboSyncState
  cardSyncState.ChangeOnEvent(combo, [this, cardSyncStatePtr] { return cardSyncStatePtr->IsReady() && cardStatePtr->SelectedNewChips(); });
  cardSyncState.ChangeOnEvent(comboSyncState, &NetworkSyncBattleState::IsReady);

  // Go to the forms state if forms are selected on either end, or go straight into battle
  comboSyncState.ChangeOnEvent(forms, [this, comboSyncStatePtr] { return comboSyncStatePtr->IsReady() && (cardStatePtr->HasForm() || remoteState.remoteChangeForm); });
  comboSyncState.ChangeOnEvent(battlestart, &NetworkSyncBattleState::IsReady);

  // Sync after combo
  combo.ChangeOnEvent(comboSyncState, &CardComboBattleState::IsDone);

  // Wait for handshake to complete by going back to the sync state..
  cardSelect.ChangeOnEvent(cardSyncState, &CardSelectBattleState::OKIsPressed);

  // Forms is the last state before kicking off the battle
  forms.ChangeOnEvent(combat, HookFormChangeEnd(forms.Unwrap(), cardSelect.Unwrap())); // handling decross during combat
  forms.ChangeOnEvent(battlestart, &CharacterTransformBattleState::IsFinished); // starting the battle after a combo sync state

  battlestart.ChangeOnEvent(combat, &BattleStartBattleState::IsFinished);
  timeFreeze.ChangeOnEvent(combat, &TimeFreezeBattleState::IsOver);

  // combat has multiple state interruptions based on events
  // so we can chain them together
  combat
    .ChangeOnEvent(battleover, HookPlayerWon(combat.Unwrap(), battleover.Unwrap()))
    .ChangeOnEvent(forms, HookPlayerDecrosses(forms.Unwrap()))
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

  connectSyncStatePtr->SetEndCallback([this](const BattleSceneState* _) {
    GetLocalPlayer()->ChangeState<PlayerControlledState>();

    if (remotePlayer) {
      remotePlayer->ChangeState<PlayerControlledState>();
    }
  });

  // setup sync signals
  connectSyncStatePtr->SetStartCallback([this](const BattleSceneState* _) { SendSyncSignal(0); });
  cardSyncStatePtr->SetStartCallback([this](const BattleSceneState* _) { SendHandshakeSignal(1); });
  comboSyncStatePtr->SetStartCallback([this](const BattleSceneState* _) { SendSyncSignal(2); });

  // this kicks-off the state graph beginning with the intro state
  this->StartStateGraph(connectSyncState);
}

NetworkBattleScene::~NetworkBattleScene() {
}

void NetworkBattleScene::OnHit(Entity& victim, const Hit::Properties& props) {
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
        Logger::Log(LogLevel::net, "Marked local player for decross");
      }
      else {
        Logger::Log(LogLevel::net, "Marked remote player for decross");
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
  if (skipFrame && FrameNumber() >= frames(5)) {
    SkipFrame();
    skippingUpdate = true;
  }
  else {

    const BattleSceneState* currentState = GetCurrentState();
    auto queueInput = currentState == startStatePtr || combatPtr->IsStateCombat(currentState);
    std::vector<InputEvent> events = ProcessLocalPlayerInputQueue(5, queueInput);

    SendFrameData(events, (FrameNumber() + frames(5)).count());
  }


  auto tiles = this->GetField()->FindTiles([](Battle::Tile*) -> bool {return true; });

  std::string skipPart1 = skippingUpdate ? "Skipping over " : "";

  std::string fieldState = skipPart1 + "Battle Frame " + std::to_string(this->FrameNumber().count()) + " tiles: ";

  for (int i = 0; i < tiles.size(); i++) {
    int state = static_cast<int>(tiles[i]->GetState());
    fieldState = fieldState + (state < 10 ? ("0" + std::to_string(state)) : std::to_string(state));
  }


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
        inp = inp + "" + events[i].name + " " + std::to_string(static_cast<int>(events[i].state)) + " ";
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

void NetworkBattleScene::onExit() {
}

void NetworkBattleScene::onEnter() {
}

void NetworkBattleScene::onStart() {
  BattleSceneBase::onStart();
  packetProcessor->EnableKickForSilence(true);
}

void NetworkBattleScene::onResume() {
}

void NetworkBattleScene::onEnd() {
  BattleSceneBase::onEnd();
  getController().SetSubtitle("");
}

const NetPlayFlags& NetworkBattleScene::GetRemoteStateFlags() {
  return remoteState;
}

const double NetworkBattleScene::GetAvgLatency() const {
  return packetProcessor->GetAvgLatency();
}

bool NetworkBattleScene::IsRemoteBehind() {
  return FrameNumber() > this->maxRemoteFrameNumber;
}

void NetworkBattleScene::Init() {
  BlockPackagePartitioner& partition = getController().BlockPackagePartitioner();

  size_t idx = 0;
  for (auto& [blocks, p, x, y] : spawnOrder) {
    if (p == GetLocalPlayer()) {
      std::string title = "Player #" + std::to_string(idx + 1);
      SpawnLocalPlayer(x, y);
      getController().SetSubtitle(title);
    }
    else {
      // Spawn and subscribe to remote player's events
      SpawnRemotePlayer(p, x, y);
    }

    // Run block programs on the remote player now that they are spawned
    for (const PackageAddress& addr : blocks) {
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

void NetworkBattleScene::SendHandshakeSignal(uint8_t syncIndex) {
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
  Logger::Log(LogLevel::net, "Form to send is " + std::to_string(form));
  unsigned thisFrame = FrameNumber().count();

  if (thisFrame > 0) {
    thisFrame = thisFrame - 1u;
  }

  size_t len = prefilteredCardSelection.size();

  Poco::Buffer<char> buffer{ 0 };
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::handshake);
  writer.Write<uint8_t>(buffer, (uint8_t)syncIndex);
  writer.Write<int32_t>(buffer, (int32_t)form);
  writer.Write<uint8_t>(buffer, (uint8_t)len);

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

    writer.WriteString<uint8_t>(buffer, id);
  }

  auto [_, id] = packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
  packetProcessor->UpdateHandshakeID(id);

  auto syncStatePtr = syncStates[syncIndex];
  syncStatePtr->MarkSyncRequested();

  if (syncStatePtr->SetSyncFrame(lastSentFrameNumber + frames(2))) {
    // + 1 in case this packet is not handled on the same frame as the input
    // + 1 again as BattleStates run after FrameIncrement
    Logger::Log(LogLevel::debug, "Using lastSentFrameNumber for sync frame");
  }
}

void NetworkBattleScene::SendSyncSignal(uint8_t syncIndex) {
  Poco::Buffer<char> buffer{ 0 };
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::sync);
  writer.Write<uint8_t>(buffer, (uint8_t)syncIndex);

  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);

  auto syncStatePtr = syncStates[syncIndex];
  syncStatePtr->MarkSyncRequested();

  if (syncStatePtr->SetSyncFrame(lastSentFrameNumber + frames(2))) {
    // + 1 in case this packet is not handled on the same frame as the input
    // + 1 again as BattleStates run after FrameIncrement
    Logger::Log(LogLevel::debug, "Using lastSentFrameNumber for sync frame");
  }
}

void NetworkBattleScene::SendFrameData(std::vector<InputEvent>& events, unsigned int frameNumber) {
  std::string inp = "Frame " + std::to_string(frameNumber) + ": Sending inputs: ";
  for (int i = 0; i < events.size(); i++) {
    inp = inp + events[i].name + " " + std::to_string(static_cast<int>(events[i].state)) + " ";
  }

  Logger::Log(LogLevel::net, inp);
  Poco::Buffer<char> buffer{ 0 };
  BufferWriter writer;
  writer.Write(buffer, NetPlaySignals::frame_data);
  writer.Write<uint32_t>(buffer, (uint32_t)frameNumber);

  // Send our hp
  int hp = 0;
  if (auto player = GetLocalPlayer()) {
    hp = player->GetHealth();
  }

  writer.Write<int32_t>(buffer, (int32_t)hp);

  // send the input keys
  writer.Write<uint8_t>(buffer, (uint8_t)events.size());

  for (auto& event : events) {
    writer.WriteString<uint8_t>(buffer, event.name);
    writer.Write(buffer, event.state);
  }

  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
  events.clear();

  lastSentFrameNumber = frames(frameNumber);
}

void NetworkBattleScene::SendPingSignal() {
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::ping };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::Reliable, buffer);
}

void NetworkBattleScene::ReceiveHandshakeSignal(const Poco::Buffer<char>& buffer) {
  if (!remoteState.remoteConnected) return;

  std::vector<std::string> remoteUUIDs;

  BufferReader reader;
  size_t syncIndex = reader.Read<uint8_t>(buffer);
  int remoteForm = reader.Read<int32_t>(buffer);
  uint8_t cardLen = reader.Read<uint8_t>(buffer);

  Logger::Logf(LogLevel::debug, "Received remote handshake. Remote sent %i cards.", (int)cardLen);
  while (cardLen > 0) {
    std::string uuid = reader.ReadString<uint8_t>(buffer);
    remoteUUIDs.push_back(uuid);
    Logger::Logf(LogLevel::debug, "Remote Card: %s", uuid.c_str());
    cardLen--;
  }

  // Now that we have the remote's form and cards,
  // populate the net play remote state with this information
  // and kick off the battle sequence

  //remoteState.remoteFormSelect = remoteForm;

  // Do not trigger a change if remoteForm is -1 (no form).
  // This avoids a desync where a decross in the previous turn and no form selected 
  // by remote in the next turn will see a changed form (X to -1).
  remoteState.remoteChangeForm = remoteForm != -1 && remoteState.remoteFormSelect != remoteForm;
  Logger::Log(LogLevel::net, "Will remote form change? " + std::to_string(remoteState.remoteChangeForm));
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
      else if (localPackageManager.HasPackage(addr.packageId)) {
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
  roundStartDelay = from_milliseconds((long long)((duration * 1000.0) + packetProcessor->GetAvgLatency()));

  // startStatePtr->SetStartupDelay(roundStartDelay);
  startStatePtr->SetStartupDelay(frames(5));

  if (syncIndex >= 0 && syncIndex < syncStates.size()) {
    auto syncStatePtr = syncStates[syncIndex];
    syncStatePtr->MarkRemoteSyncRequested();

    if (syncStatePtr->SetSyncFrame(maxRemoteFrameNumber + frames(2))) {
      // + 1 in case this packet is not handled on the same frame as the input
      // + 1 again as BattleStates run after FrameIncrement
      Logger::Log(LogLevel::debug, "Using maxRemoteFrameNumber for sync frame");
    }
  }
}

void NetworkBattleScene::ReceiveSyncSignal(const Poco::Buffer<char>& buffer) {
  BufferReader reader;
  size_t syncIndex = reader.Read<uint8_t>(buffer);

  if (syncIndex >= 0 && syncIndex < syncStates.size()) {
    auto syncStatePtr = syncStates[syncIndex];
    syncStatePtr->MarkRemoteSyncRequested();

    if (syncStatePtr->SetSyncFrame(maxRemoteFrameNumber + frames(2))) {
      // + 1 in case this packet is not handled on the same frame as the input
      // + 1 again as BattleStates run after FrameIncrement
      Logger::Log(LogLevel::debug, "Using maxRemoteFrameNumber for sync frame");
    }
  }
}

void NetworkBattleScene::ReceiveFrameData(const Poco::Buffer<char>& buffer) {
  if (!remotePlayer) return;

  BufferReader reader;
  unsigned int frameNumber = reader.Read<uint32_t>(buffer);

  Logger::Log(LogLevel::net, "Received remote inputs for " + std::to_string(frameNumber));
  maxRemoteFrameNumber = frames(frameNumber);

  int hp = reader.Read<int32_t>(buffer);

  size_t list_len = reader.Read<uint8_t>(buffer);

  std::vector<InputEvent> events;
  while (list_len-- > 0) {
    InputEvent event{};
    event.name = reader.ReadString<uint8_t>(buffer);
    event.state = reader.Read<InputState>(buffer);
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

void NetworkBattleScene::SpawnRemotePlayer(std::shared_ptr<Player> newRemotePlayer, int x, int y) {
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

void NetworkBattleScene::OnSelectNewCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards) {
  // intercept pre-filtered cards to send them over the network
  if (player == GetLocalPlayer()) {
    prefilteredCardSelection.clear();

    for (Battle::Card& card : cards) {
      prefilteredCardSelection.push_back(card.GetUUID());
    }
  }
}

std::function<bool()> NetworkBattleScene::HookPlayerWon(CombatBattleState& combat, BattleOverBattleState& over) {
  auto lambda = [&combat, &over, this] {
    bool result = GetLocalPlayer()->GetTeam() == Team::red ? combat.RedTeamWon() : combat.BlueTeamWon();

    if (result) {
      over.SetIntroText("Enemy Deleted!");
    }

    return result;
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookPlayerLost(CombatBattleState& combat, BattleOverBattleState& over) {
  auto lambda = [&combat, &over, this] {
    bool result = combat.PlayerDeleted();

    if (result) {
      over.SetIntroText(GetLocalPlayer()->GetName() + " Deleted!");
    }

    return result;
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookPlayerDecrosses(CharacterTransformBattleState& forms) {
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

std::function<bool()> NetworkBattleScene::HookOnCardSelectEvent() {
  // Lambda event callback that captures and handles network card select screen opening
  auto lambda = [this]() mutable {
    bool remoteRequestedChipSelect = remotePlayer && remotePlayer->InputState().Has(InputEvents::pressed_cust_menu);
    return combatPtr->PlayerRequestCardSelect() || (remoteRequestedChipSelect && this->IsCustGaugeFull());
  };

  return lambda;
}

std::function<bool()> NetworkBattleScene::HookFormChangeEnd(CharacterTransformBattleState& form, CardSelectBattleState& cardSelect) {
  auto lambda = [&form, &cardSelect, this]() mutable {
    Logger::Log(LogLevel::net, "Check for form change end");
    bool localTriggered = (GetLocalPlayer()->GetHealth() == 0 || localPlayerDecross);
    bool remoteTriggered = (remotePlayer->GetHealth() == 0 || remotePlayerDecross);
    bool triggered = form.IsFinished() && (localTriggered || remoteTriggered);

    if (triggered) {
      Logger::Log(LogLevel::net, "Really ended");

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


void NetworkBattleScene::ProcessPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body) {
  try {
    switch (header) {
    case NetPlaySignals::handshake:
      ReceiveHandshakeSignal(body);
      break;
    case NetPlaySignals::sync:
      ReceiveSyncSignal(body);
      break;
    case NetPlaySignals::frame_data:
      ReceiveFrameData(body);
      break;
    }
  }
  catch (std::exception& e) {
    Logger::Logf(LogLevel::critical, "PVP Network exception: %s", e.what());
    packetProcessor->HandleError();
  }
}

void NetworkBattleScene::UpdatePingIndicator(frame_time_t frames) {
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
  else {
    // 4th frame is excellent or zero latency
    idx = 4;
  }

  pingIndicator.setTextureRect(sf::IntRect((idx - 1u) * 16, 0, 16, 16));
}