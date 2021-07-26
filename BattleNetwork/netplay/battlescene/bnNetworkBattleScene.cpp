#include <Swoosh/ActivityController.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/PixelateBlackWashFade.h>
#include <chrono>

#include "bnNetworkBattleScene.h"
#include "../bnPlayerInputReplicator.h"
#include "../bnPlayerNetworkState.h"
#include "../bnPlayerNetworkProxy.h"
#include "../../bnFadeInState.h"
#include "../../bnElementalDamage.h"

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

NetworkBattleScene::NetworkBattleScene(ActivityController& controller, const NetworkBattleSceneProps& props, BattleResultsFunc onEnd) :
  BattleSceneBase(controller, props.base, onEnd),
  ping(Font::Style::wide)
{
  ping.setPosition(480 - (2.f * 16) - 4, 320 - 2.f); // screen upscaled w - (16px*upscale scale) - (2px*upscale)
  ping.SetColor(sf::Color::Red);

  pingIndicator.setTexture(Textures().LoadTextureFromFile("resources/ui/ping.png"));
  pingIndicator.getSprite().setOrigin(sf::Vector2f(16.f, 16.f));
  pingIndicator.setPosition(480, 320);
  pingIndicator.setScale(2.f, 2.f);
  UpdatePingIndicator(frames(0));

  auto* clientPlayer = &props.base.player;

  selectedNavi = props.netconfig.myNavi;
  props.base.player.CreateComponent<PlayerInputReplicator>(clientPlayer);

  packetProcessor = props.packetProcessor;
  packetProcessor->SetKickCallback([this] {
    this->Quit(FadeOut::black);
  });

  packetProcessor->SetPacketBodyCallback([this](NetPlaySignals header, const Poco::Buffer<char>& buffer) {
    this->processPacketBody(header, buffer);
  });

  // If playing co-op, add more players to track here
  players = { clientPlayer };

  // ptr to player, form select index (-1 none), if should transform
  // TODO: just make this a struct to use across all states that need it...
  trackedForms = {
    std::make_shared<TrackedFormData>(clientPlayer, -1, false)
  };

  // trackedForms[0]->SetReadyState<PlayerControlledState>();

  // in seconds
  double battleDuration = 10.0;

  mob = new Mob(props.base.field);

  // First, we create all of our scene states
  auto syncState = AddState<NetworkSyncBattleState>(remotePlayer, this);
  auto cardSelect = AddState<CardSelectBattleState>(players, trackedForms);
  auto combat = AddState<CombatBattleState>(mob, players, battleDuration);
  auto combo = AddState<CardComboBattleState>(this->GetSelectedCardsUI(), props.base.programAdvance);
  auto forms = AddState<CharacterTransformBattleState>(trackedForms);
  auto battlestart = AddState<BattleStartBattleState>(players);
  auto battleover = AddState<BattleOverBattleState>(players);
  auto timeFreeze = AddState<TimeFreezeBattleState>();
  auto fadeout = AddState<FadeOutBattleState>(FadeOut::black, players); // this state requires arguments

  // We need to respond to new events later, create a resuable pointer to these states
  timeFreezePtr = &timeFreeze.Unwrap();
  combatPtr = &combat.Unwrap();
  syncStatePtr = &syncState.Unwrap();
  cardComboStatePtr = &combo.Unwrap();
  startStatePtr = &battlestart.Unwrap();
  cardStatePtr = &cardSelect.Unwrap();

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
  forms.ChangeOnEvent(battlestart, &CharacterTransformBattleState::IsFinished);
  battlestart.ChangeOnEvent(combat, &BattleStartBattleState::IsFinished);
  timeFreeze.ChangeOnEvent(combat, &TimeFreezeBattleState::IsOver);

  // special condition: if lost in combat and had a form, trigger the character transform states
  auto playerLosesInForm = [this] {
    const bool changeState = this->trackedForms[0]->player->GetHealth() == 0 && (this->trackedForms[0]->selectedForm != -1);

    if (changeState) {
      this->trackedForms[0]->selectedForm = -1;
      this->trackedForms[0]->animationComplete = false;
    }

    return changeState;
  };

  // Lambda event callback that captures and handles network card select screen opening
  auto onCardSelectEvent = [this]() mutable {
    if (combatPtr->PlayerRequestCardSelect() || waitingForCardSelectScreen) {
      if (!waitingForCardSelectScreen) {
        cardStateDelay = from_milliseconds(packetProcessor->GetAvgLatency());
        this->sendRequestedCardSelectSignal();
        waitingForCardSelectScreen = true;
      }

      if (cardStateDelay <= frames(0) && waitingForCardSelectScreen) {
        waitingForCardSelectScreen = false;
      }

      return !waitingForCardSelectScreen;
    }
    else if (remoteState.openedCardWidget) {
      remoteState.openedCardWidget = false;
      return true;
    }

    return false;
  };

  // combat has multiple state interruptions based on events
  // so we can chain them together
  combat
    .ChangeOnEvent(battleover, &CombatBattleState::PlayerWon)
    //.ChangeOnEvent(forms, playerLosesInForm)
    .ChangeOnEvent(fadeout, &CombatBattleState::PlayerLost)
    .ChangeOnEvent(cardSelect, onCardSelectEvent)
    .ChangeOnEvent(timeFreeze, &CombatBattleState::HasTimeFreeze);

  battleover.ChangeOnEvent(fadeout, &BattleOverBattleState::IsFinished);

  // share some values between states
  combo->ShareCardList(&cardSelect->GetCardPtrList(), &cardSelect->GetCardListLengthAddr());

  // Some states need to know about card uses
  auto& ui = this->GetSelectedCardsUI();
  ui.Reveal();
  
  GetCardSelectWidget().SetSpeaker(props.mug, props.anim);
  GetEmotionWindow().SetTexture(props.emotion);

  combat->Subscribe(ui);
  timeFreeze->Subscribe(ui);

  // pvp cannot pause
  combat->EnablePausing(false);

  // Some states are part of the combat routine and need to respect
  // the combat state's timers
  combat->subcombatStates.push_back(&timeFreeze.Unwrap());

  // this kicks-off the state graph beginning with the intro state
  this->StartStateGraph(syncState);

  sendConnectSignal(this->selectedNavi); // NOTE: this function only happens once at start
}

NetworkBattleScene::~NetworkBattleScene()
{ 
  delete remotePlayer;
  if (remoteHand) delete[] remoteHand;
}

void NetworkBattleScene::OnHit(Character& victim, const Hit::Properties& props)
{
  auto player = GetPlayer();
  if (player == &victim && props.damage > 0) {
    if (props.damage >= 300) {
      player->SetEmotion(Emotion::angry);
      GetSelectedCardsUI().SetMultiplier(2);
    }

    if (player->IsSuperEffective(props.element)) {
      // deform
      trackedForms[0]->animationComplete = false;
      trackedForms[0]->selectedForm = -1;
    }
  }

  if (victim.IsSuperEffective(props.element) && props.damage > 0) {
    Artifact* seSymbol = new ElementalDamage;
    seSymbol->SetLayer(-100);
    seSymbol->SetHeight(victim.GetHeight() + (victim.getLocalBounds().height * 0.5f)); // place it at sprite height
    GetField()->AddEntity(*seSymbol, victim.GetTile()->GetX(), victim.GetTile()->GetY());
  }
}

void NetworkBattleScene::onUpdate(double elapsed) {
  
  if (!IsSceneInFocus()) {
    if (IsQuitting()) {
      // net play packet processor is shared with other scenes
      // we don't want to delete it, we just want to ignore packets coming in
      // so we don't point to a dead scene
      ResetPacketProcessor();
    }

    return;
  }

  BattleSceneBase::onUpdate(elapsed);

  if (!this->IsPlayerDeleted()) {
    sendHPSignal(GetPlayer()->GetHealth());
  }

  frame_time_t elapsed_frames = from_seconds(elapsed);

  if (cardStateDelay > frames(0)) {
    cardStateDelay -= elapsed_frames;
  }

  if (!syncStatePtr->IsSynchronized()) {
    if (packetProcessor->IsHandshakeAck() && remoteState.remoteHandshake) {
      syncStatePtr->Synchronize();
    }
  }
  else {
    packetTime += elapsed_frames;
  }

  if (remotePlayer && remotePlayer->WillRemoveLater()) {
    auto iter = std::find(players.begin(), players.end(), remotePlayer);
    if (iter != players.end()) {
      players.erase(iter);
    }
    remoteState.remoteConnected = false;
    remotePlayer = nullptr;
  }

  if (auto player = GetPlayer()) {
    BattleResultsObj().finalEmotion = player->GetEmotion();
  }
}

void NetworkBattleScene::onDraw(sf::RenderTexture& surface) {
  BattleSceneBase::onDraw(surface);

  auto lag = GetAvgLatency();

  // Logger::Logf("lag is: %f", lag);

  // draw network ping
  ping.SetString(std::to_string(lag).substr(0, 5));

  //the bounds has been changed after changing the text
  auto pbounds = ping.GetLocalBounds();
  ping.setOrigin(pbounds.width, pbounds.height);

  // draw
  surface.draw(ping);

  // convert from ms to seconds to discrete frame count...
  frame_time_t lagTime = frame_time_t{ (long long)(lag) };

  if (remoteState.remoteHandshake) {
    // factor in how long inputs are not being ack'd after sent
    UpdatePingIndicator(std::max(packetTime, lagTime));
  }
  else {
    // use ack to determine connectivity here
    UpdatePingIndicator(lagTime);
  }

  //draw
  surface.draw(pingIndicator);
}

void NetworkBattleScene::onExit()
{
  
}
/*!
 * @brief 
*/
void NetworkBattleScene::onEnter()
{
}

void NetworkBattleScene::onStart()
{
  BattleSceneBase::onStart();

  // Once the transition completes, we begin handshakes
  packetProcessor->EnableKickForSilence(true);

}

void NetworkBattleScene::onResume()
{
}

void NetworkBattleScene::onEnd()
{
  BattleSceneBase::onEnd();
}

void NetworkBattleScene::Inject(PlayerInputReplicator& pub)
{
  pub.Inject(*this);
}

const NetPlayFlags& NetworkBattleScene::GetRemoteStateFlags()
{
  return remoteState; 
}

const double NetworkBattleScene::GetAvgLatency() const
{
  return packetProcessor->GetAvgLatency();
}

void NetworkBattleScene::sendHandshakeSignal()
{
  /**
  To begin the round, we need to supply the following information to our opponent:
    1. Our selected form
    2. Our card list size
    3. Our card list items

  We need to also measure latency to synchronize client animation with
  remote animations (see: combos and forms)
  */

  int form = trackedForms[0]->selectedForm;
  auto& selectedCardsWidget = this->GetSelectedCardsUI();
  std::vector<std::string> uuids = selectedCardsWidget.GetUUIDList();
  size_t len = uuids.size();

  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals signalType{ NetPlaySignals::handshake };
  buffer.append((char*)&signalType, sizeof(NetPlaySignals));
  buffer.append((char*)&form, sizeof(int));
  buffer.append((char*)&len, sizeof(size_t));

  for (auto& id : uuids) {
    size_t len = id.size();
    buffer.append((char*)&len, sizeof(size_t));
    buffer.append(id.c_str(), len);
  }

  auto [_, id] = packetProcessor->SendPacket(Reliability::Reliable, buffer);
  packetProcessor->UpdateHandshakeID(id);
}

void NetworkBattleScene::sendInputEvents(const std::vector<InputEvent>& events)
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals signalType{ NetPlaySignals::input_event };
  buffer.append((char*)&signalType, sizeof(NetPlaySignals));

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
}

void NetworkBattleScene::sendConnectSignal(const SelectedNavi navi)
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::connect };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  buffer.append((char*)&navi, sizeof(SelectedNavi));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::sendChangedFormSignal(const int form)
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::form };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  buffer.append((char*)&form, sizeof(int));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::sendHPSignal(const int hp)
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::hp };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  buffer.append((char*)&hp, sizeof(int));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::sendTileCoordSignal(const int x, const int y)
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::tile };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  buffer.append((char*)&x, sizeof(int));
  buffer.append((char*)&y, sizeof(int));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::sendRequestedCardSelectSignal()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::card_select };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::sendLoserSignal()
{
  Poco::Buffer<char> buffer{ 0 };
  NetPlaySignals type{ NetPlaySignals::loser };
  buffer.append((char*)&type, sizeof(NetPlaySignals));
  packetProcessor->SendPacket(Reliability::ReliableOrdered, buffer);
}

void NetworkBattleScene::recieveHandshakeSignal(const Poco::Buffer<char>& buffer)
{
  std::vector<std::string> remoteUUIDs;
  int remoteForm{ -1 };
  size_t cardLen{};
  size_t read{};

  std::memcpy(&remoteForm, buffer.begin(), sizeof(int));
  read += sizeof(int);

  std::memcpy(&cardLen, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

  while (cardLen > 0) {
    std::string uuid;
    size_t len{};
    std::memcpy(&len, buffer.begin() + read, sizeof(size_t));
    read += sizeof(size_t);
    uuid.resize(len);
    std::memcpy(uuid.data(), buffer.begin() + read, len);
    read += len;
    remoteUUIDs.push_back(uuid);
    cardLen--;
  }

  // Now that we have the remote's form and cards,
  // populate the net play remote state with this information
  // and kick off the battle sequence

  //remoteState.remoteFormSelect = remoteForm;
  remoteState.remoteChangeForm = remoteState.remoteFormSelect != remoteForm;
  remoteState.remoteFormSelect = remoteForm;
  trackedForms[1]->selectedForm = remoteForm;
  trackedForms[1]->animationComplete = !remoteState.remoteChangeForm; // a value of false forces animation to play

  if (remoteHand) {
    delete[] remoteHand;
    remoteHand = nullptr;
  }

  size_t handSize = remoteUUIDs.size();
  int len = (int)handSize; // TODO: use size_t for card lengths...

  if (handSize) {
    remoteHand = new Battle::Card * [handSize];

    for (size_t i = 0; i < remoteUUIDs.size(); i++) {
      Battle::Card card = WEBCLIENT.MakeBattleCardFromWebCardData(WebAccounts::Card{ remoteUUIDs[i] });
      remoteHand[i] = new Battle::Card(card);
    }
  }

  // Prepare for simulation
  cardComboStatePtr->Reset();
  double duration{};

  // simulate PA to calculate time required to animate
  while (!cardComboStatePtr->IsDone()) {
    constexpr double step = 1.0 / frame_time_t::frames_per_second;
    cardComboStatePtr->Simulate(step, &remoteHand, &len, false);
    duration += step;
  }

  // Prepare for next PA
  cardComboStatePtr->Reset();

  // Filter support cards
  FilterSupportCards(remoteHand, len);

  // Supply the final hand info
  remoteCardUsePublisher->LoadCards(remoteHand, len);
  
  // Convert to microseconds and use this as the round start delay
  roundStartDelay = from_milliseconds((long long)((duration*1000.0) + packetProcessor->GetAvgLatency()));

  startStatePtr->SetStartupDelay(roundStartDelay);

  remoteState.remoteHandshake = true;
}

void NetworkBattleScene::recieveInputEvent(const Poco::Buffer<char>& buffer)
{
  remoteState.remoteInputEvents.clear();

  std::string name;
  size_t len{}, list_len{};
  size_t read{};
  std::memcpy(&list_len, buffer.begin() + read, sizeof(size_t));
  read += sizeof(size_t);

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
    remoteState.remoteInputEvents.push_back(event);
  }

}

void NetworkBattleScene::recieveConnectSignal(const Poco::Buffer<char>& buffer)
{
  if (remoteState.remoteConnected) return; // prevent multiple connection requests...

  remoteState.remoteConnected = true;

  SelectedNavi navi = SelectedNavi{ 0 }; 

  if (buffer.size() >= sizeof(SelectedNavi)) {
    std::memcpy(&navi, buffer.begin(), sizeof(SelectedNavi));
  }
  else {
    std::string str_buffer(buffer.begin(), buffer.end());
    Logger::Logf("Incoming connect signal was corrupted: %s", str_buffer.c_str());
    return;
  }

  remoteState.remoteNavi = navi;

  Logger::Logf("Recieved connect signal! Remote navi: %i", remoteState.remoteNavi);

  assert(remotePlayer == nullptr && "remote player was already set!");
  remotePlayer = NAVIS.At(navi).GetNavi();

  // TODO: manual flipping shouldn't be needed. The engine should flip based on team and direction...
  remotePlayer->SetTeam(Team::blue);
  remotePlayer->setScale(remotePlayer->getScale().x * -1.0f, remotePlayer->getScale().y);

  remotePlayer->ChangeState<FadeInState<Player>>([]{});

  GetField()->AddEntity(*remotePlayer, remoteState.remoteTileX, remoteState.remoteTileY);
  mob->Track(*remotePlayer);

  remoteCardUsePublisher = remotePlayer->CreateComponent<PlayerSelectedCardsUI>(remotePlayer);
  remoteCardUsePublisher->Hide(); // do not reveal opponent's cards
  combatPtr->Subscribe(*remoteCardUsePublisher);
  timeFreezePtr->Subscribe(*remoteCardUsePublisher);
  this->SubscribeToCardEvents(*remoteCardUsePublisher);

  remotePlayer->CreateComponent<MobHealthUI>(remotePlayer);
  auto netProxy = remotePlayer->CreateComponent<PlayerNetworkProxy>(remotePlayer, remoteState);

  players.push_back(remotePlayer);
  trackedForms.push_back(std::make_shared<TrackedFormData>(remotePlayer, -1, false ));
  //trackedForms.back()->SetReadyState<PlayerNetworkState>(netProxy);

  LoadMob(*mob);
}

void NetworkBattleScene::recieveChangedFormSignal(const Poco::Buffer<char>& buffer)
{
  if (buffer.empty()) return;
  int form = remoteState.remoteFormSelect;
  int prevForm = remoteState.remoteFormSelect;
  std::memcpy(&form, buffer.begin(), sizeof(int));

  if (remotePlayer && form != prevForm) {
    remoteState.remoteFormSelect = form;

    // TODO: hacky and ugly
    this->trackedForms[1]->selectedForm = remoteState.remoteFormSelect;
    this->trackedForms[1]->animationComplete = false; // kick-off animation
  }
}

void NetworkBattleScene::recieveHPSignal(const Poco::Buffer<char>& buffer)
{
  if (buffer.empty()) return;
  if (!remoteState.remoteConnected) return;

  int hp = remotePlayer->GetHealth();
  std::memcpy(&hp, buffer.begin(), sizeof(int));
  
  remoteState.remoteHP = hp;
  remotePlayer->SetHealth(hp);
}

void NetworkBattleScene::recieveTileCoordSignal(const Poco::Buffer<char>& buffer)
{
  if (buffer.empty()) return;
  if (!remoteState.remoteConnected) return;

  int x = remoteState.remoteTileX; 
  std::memcpy(&x, buffer.begin(), sizeof(int));

  int y = remoteState.remoteTileX; 
  std::memcpy(&y, (buffer.begin()+sizeof(int)), sizeof(int));

  // mirror the x value for remote
  x = (GetField()->GetWidth() - x)+1;

  remoteState.remoteTileX = x;
  remoteState.remoteTileY = y;
}

void NetworkBattleScene::recieveLoserSignal()
{
  // TODO: replace this with PVP win information
  packetProcessor->EnableKickForSilence(false);
  this->Quit(FadeOut::black);
}

void NetworkBattleScene::recieveRequestedCardSelectSignal()
{
  // also going to trigger opening the card select widget
  remoteState.openedCardWidget = true;
}

void NetworkBattleScene::processPacketBody(NetPlaySignals header, const Poco::Buffer<char>& body)
{
  try {
    switch (header) {
      case NetPlaySignals::handshake:
        recieveHandshakeSignal(body);
        break;
      case NetPlaySignals::connect:
        recieveConnectSignal(body);
        break;
      case NetPlaySignals::input_event:
        recieveInputEvent(body);
        break;
      case NetPlaySignals::form:
        recieveChangedFormSignal(body);
        break;
      case NetPlaySignals::hp:
        recieveHPSignal(body);
        break;
      case NetPlaySignals::loser:
        recieveLoserSignal();
        break;
      case NetPlaySignals::tile:
        recieveTileCoordSignal(body);
        break;
      case NetPlaySignals::card_select:
        recieveRequestedCardSelectSignal();
        break;
    }
  }
  catch (std::exception& e) {
    Logger::Logf("PVP Network exception: %s", e.what());
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

void NetworkBattleScene::ResetPacketProcessor()
{
  packetProcessor->EnableKickForSilence(false);
  packetProcessor->SetKickCallback(Netplay::PacketProcessor::KickFunc{});
  packetProcessor->SetPacketBodyCallback(Netplay::PacketProcessor::PacketbodyFunc{});
}
