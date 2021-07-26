#include "bnBattleSceneBase.h"

#include <assert.h>
#include <Segues/WhiteWashFade.h>
#include <Segues/BlackWashFade.h>
#include <Segues/PixelateBlackWashFade.h>

#include "../bnTextureResourceManager.h"
#include "../bnShaderResourceManager.h"
#include "../bnInputManager.h"
#include "../bnMob.h"
#include "../bnCardAction.h"
#include "../bnPlayerHealthUI.h"
#include "../bnPlayerEmotionUI.h"
#include "../bnUndernetBackground.h"
#include "../bnWeatherBackground.h"
#include "../bnRobotBackground.h"
#include "../bnMedicalBackground.h"
#include "../bnACDCBackground.h"
#include "../bnMiscBackground.h"
#include "../bnSecretBackground.h"
#include "../bnJudgeTreeBackground.h"
#include "../bnLanBackground.h"
#include "../bnGraveyardBackground.h"
#include "../bnVirusBackground.h"

// Combos are counted if more than one enemy is hit within x frames
// The game is clocked to display 60 frames per second
// If x = 20 frames, then we want a combo hit threshold of 20/60 = 0.3 seconds
#define COMBO_HIT_THRESHOLD_SECONDS 20.0f/60.0f

using swoosh::types::segue;
using swoosh::Activity;
using swoosh::ActivityController;

BattleSceneBase::BattleSceneBase(ActivityController& controller, const BattleSceneBaseProps& props, BattleResultsFunc onEnd) :
  Scene(controller),
  player(&props.player),
  programAdvance(props.programAdvance),
  comboDeleteCounter(0),
  totalCounterMoves(0),
  totalCounterDeletions(0),
  whiteShader(*Shaders().GetShader(ShaderType::WHITE_FADE)),
  backdropShader(*Shaders().GetShader(ShaderType::BLACK_FADE)),
  yellowShader(*Shaders().GetShader(ShaderType::YELLOW)),
  heatShader(*Shaders().GetShader(ShaderType::SPOT_DISTORTION)),
  iceShader(*Shaders().GetShader(ShaderType::SPOT_REFLECTION)),
  cardListener(),
  // cap of 8 cards, 8 cards drawn per turn
  cardCustGUI({ props.folder, 8, 8 }),
  mobFont(Font::Style::thick),
  camera(sf::View{ sf::Vector2f(240, 160), sf::Vector2f(480, 320) }),
  onEndCallback(onEnd),
  channel(this)
{
  /*
  Set Scene*/
  field = props.field;
  CharacterDeleteListener::Subscribe(*field);
  field->SetScene(this); // event emitters during battle needs the active scene

  player->ChangeState<PlayerIdleState>();
  player->ToggleTimeFreeze(false);
  field->AddEntity(*player, 2, 2);

  /*
  Background for scene*/
  background = props.background;

  if (!background) {
    int randBG = rand() % 8;

    if (randBG == 0) {
      background = std::make_shared<LanBackground>();
    }
    else if (randBG == 1) {
      background = std::make_shared<GraveyardBackground>();
    }
    else if (randBG == 2) {
      background = std::make_shared<WeatherBackground>();
    }
    else if (randBG == 3) {
      background = std::make_shared<RobotBackground>();
    }
    else if (randBG == 4) {
      background = std::make_shared<MedicalBackground>();
    }
    else if (randBG == 5) {
      background = std::make_shared<ACDCBackground>();
    }
    else if (randBG == 6) {
      background = std::make_shared<MiscBackground>();
    }
    else if (randBG == 7) {
      background = std::make_shared<JudgeTreeBackground>();
    }
  }

  // Card UI for player
  cardUI = player->CreateComponent<PlayerSelectedCardsUI>(player);
  // cardListener.Subscribe(*cardUI);
  this->CardUseListener::Subscribe(*cardUI);

  // Player UI 
  auto healthUI = player->CreateComponent<PlayerHealthUI>(player);
  cardCustGUI.AddNode(healthUI);

  // Player Emotion
  this->emotionUI = player->CreateComponent<PlayerEmotionUI>(player);
  cardCustGUI.AddNode(emotionUI);

  emotionUI->Subscribe(cardCustGUI);

  /*
  Counter "reveal" ring
  */

  counterRevealAnim = Animation("resources/navis/counter_reveal.animation");
  counterRevealAnim << "DEFAULT" << Animator::Mode::Loop;

  counterReveal.setTexture(LOAD_TEXTURE(MISC_COUNTER_REVEAL), true);
  counterReveal.EnableParentShader(false);
  counterReveal.SetLayer(-100);

  counterCombatRule = new CounterCombatRule(this);

  // Load forms
  cardCustGUI.SetPlayerFormOptions(player->GetForms());

  // MOB UI
  mobBackdropSprite = sf::Sprite(*LOAD_TEXTURE(MOB_NAME_BACKDROP));
  mobEdgeSprite = sf::Sprite(*LOAD_TEXTURE(MOB_NAME_EDGE));

  mobBackdropSprite.setScale(2.f, 2.f);
  mobEdgeSprite.setScale(2.f, 2.f);

  // SHADERS
  // TODO: Load shaders if supported
  shaderCooldown = 0;

  whiteShader.setUniform("texture", sf::Shader::CurrentTexture);
  whiteShader.setUniform("opacity", 0.5f);
  whiteShader.setUniform("texture", sf::Shader::CurrentTexture);

  textureSize = getController().getVirtualWindowSize();

  iceShader.setUniform("texture", sf::Shader::CurrentTexture);
  iceShader.setUniform("sceneTexture", sf::Shader::CurrentTexture);
  iceShader.setUniform("textureSizeIn", sf::Glsl::Vec2((float)textureSize.x, (float)textureSize.y));
  iceShader.setUniform("shine", 0.2f);

  isSceneInFocus = false;

  comboInfoTimer.start();
  multiDeleteTimer.start();

  HitListener::Subscribe(*player);

  setView(sf::Vector2u(480, 320));

  // add the camera to our event bus
  channel.Register(&camera);
}

BattleSceneBase::~BattleSceneBase() {
  // drop the camera from our event bus
  channel.Drop(&camera);

  for (auto&& elem : states) {
    delete elem;
  }

  for (auto&& elem : nodeToEdges) {
    delete elem.second;
  }
}

const bool BattleSceneBase::DoubleDelete() const
{
  return didDoubleDelete;
}
const bool BattleSceneBase::TripleDelete() const
{
  return didTripleDelete;
}
;

const int BattleSceneBase::GetCounterCount() const {
  return totalCounterMoves;
}

const bool BattleSceneBase::IsSceneInFocus() const
{
  return isSceneInFocus;
}

const bool BattleSceneBase::IsQuitting() const
{
    return this->quitting;
}

void BattleSceneBase::OnCounter(Character& victim, Character& aggressor)
{
  Audio().Play(AudioType::COUNTER, AudioPriority::highest);

  if (&aggressor == player) {
    totalCounterMoves++;

    if (victim.IsDeleted()) {
      totalCounterDeletions++;
    }

    didCounterHit = true;
    comboInfoTimer.reset();

    if (player->IsInForm() == false && player->GetEmotion() != Emotion::evil) {
      field->RevealCounterFrames(true);

      // node positions are relative to the parent node's origin
      auto bounds = player->getLocalBounds();
      counterReveal.setPosition(0, -bounds.height / 4.0f);
      player->AddNode(&counterReveal);

      cardUI->SetMultiplier(2);

      player->SetEmotion(Emotion::full_synchro);

      // when players get hit by impact, battle scene takes back counter blessings
      player->AddDefenseRule(counterCombatRule);
    }
  }
}

void BattleSceneBase::OnDeleteEvent(Character& pending)
{
  // Track if player is being deleted
  if (!isPlayerDeleted && player == &pending) {
    isPlayerDeleted = true;
    player = nullptr;
  }

  auto pendingPtr = &pending;

  // Find any AI using this character as a target and free that pointer  
  field->FindEntities([pendingPtr](Entity* in) {
    auto agent = dynamic_cast<Agent*>(in);

    if (agent && agent->GetTarget() == pendingPtr) {
      agent->FreeTarget();
    }

    return false;
  });

  Logger::Logf("Removing %s from battle (ID: %d)", pending.GetName().c_str(), pending.GetID());
  mob->Forget(pending);

  if (mob->IsCleared()) {
    Audio().StopStream();
  }
}

const int BattleSceneBase::ComboDeleteSize() const 
{
  return comboInfoTimer.getElapsed().asSeconds() <= 1.0f ? comboDeleteCounter : 0;
}

const bool BattleSceneBase::Countered() const
{
  return comboInfoTimer.getElapsed().asSeconds() <= 1.0f && didCounterHit;
}

void BattleSceneBase::HighlightTiles(bool enable)
{
  this->highlightTiles = enable;
}

const double BattleSceneBase::GetCustomBarProgress() const
{
  return this->customProgress;
}

const double BattleSceneBase::GetCustomBarDuration() const
{
  return this->customDuration;
}

void BattleSceneBase::SetCustomBarProgress(double percentage)
{
  this->customProgress = this->customDuration * percentage;
}

void BattleSceneBase::SetCustomBarDuration(double maxTimeSeconds)
{
  this->customDuration = maxTimeSeconds;
}

void BattleSceneBase::SubscribeToCardEvents(CardUsePublisher& publisher)
{
  cardListener.Subscribe(publisher);
  this->CardUseListener::Subscribe(publisher);
  cardUseSubscriptions.push_back(std::ref(publisher));
}

void BattleSceneBase::UnsubscribeFromCardEvents(CardUsePublisher& publisher)
{
  // todo: cardListener.Unsubscribe(publisher);
}

const std::vector<std::reference_wrapper<CardUsePublisher>>& BattleSceneBase::GetCardUseSubscriptions() const
{
  return cardUseSubscriptions;
}

void BattleSceneBase::OnCardUse(const Battle::Card& card, Character& user, long long timestamp)
{
  HandleCounterLoss(user, true);
}

void BattleSceneBase::LoadMob(Mob& mob)
{
  this->mob = &mob;
}

void BattleSceneBase::HandleCounterLoss(Character& subject, bool playsound)
{
  if (&subject == player) {
    if (field->DoesRevealCounterFrames()) {
      player->RemoveNode(&counterReveal);
      player->RemoveDefenseRule(counterCombatRule);
      player->SetEmotion(Emotion::normal);
      field->RevealCounterFrames(false);

      playsound ? Audio().Play(AudioType::COUNTER_BONUS, AudioPriority::highest) : 0;
    }
    cardUI->SetMultiplier(1);
  }
}

void BattleSceneBase::FilterSupportCards(Battle::Card** cards, int& cardCount) {
  // Only remove the support cards in the queue. Increase the previous card damage by their support value.
  int newCardCount = cardCount;
  Battle::Card* card = nullptr;

  // Create a temp card list
  Battle::Card** newCardList = new Battle::Card*[cardCount];

  int j = 0;
  for (int i = 0; i < cardCount; ) {
    if (cards[i]->IsBooster()) {
      Logger::Logf("Booster card %s detected", cards[i]->GetShortName().c_str());

      // check if we are tracking a non-booster card first
      if (card) {
        // booster cards do not modify other booster cards
        if (!card->IsBooster()) {
          int lastDamage = card->GetDamage();
          int buff = 0;

          // NOTE: hardcoded filter step for "Atk+X" cards
          if (cards[i]->GetShortName().substr(0, 3) == "Atk") {
            std::string substr = cards[i]->GetShortName().substr(4, cards[i]->GetShortName().size() - 4).c_str();
            buff = atoi(substr.c_str());
          }

          card->ModDamage(buff);
        }

        i++;
        continue; // skip the rest of the code below
      }
    }

    newCardList[j] = cards[i];
    card = cards[i];

    i++;
    j++;
  }

  newCardCount = j;

  // Set the new cards
  for (int i = 0; i < newCardCount; i++) {
    cards[i] = *(newCardList + i);
  }

  // Set the new card count
  cardCount = newCardCount;

  // Delete the temp list space
  // NOTE: We are _not_ deleting the pointers in them
  delete[] newCardList;
}

#ifdef __ANDROID__
void BattleSceneBase::SetupTouchControls() {

}

void BattleSceneBase::ShutdownTouchControls() {

}
#endif

void BattleSceneBase::StartStateGraph(StateNode& start) {
  // kick-off and align all sprites on the screen
  field->Update(0);

  // set the current state ptr and kick-off
  this->current = &start.state;
  this->current->onStart();
}

void BattleSceneBase::onStart()
{
  isSceneInFocus = true;

  // Stream battle music
  if (mob && mob->HasCustomMusicPath()) {
    Audio().Stream(mob->GetCustomMusicPath(), true);
  }
  else {
    if (mob == nullptr || !mob->IsBoss()) {
      sf::Music::TimeSpan span;
      span.offset = sf::milliseconds(84);
      span.length = sf::seconds(120.0f * 1.20668f);

      Audio().Stream("resources/loops/loop_battle.ogg", true, span);
    }
    else {
      Audio().Stream("resources/loops/loop_boss_battle.ogg", true);
    }
  }
}

void BattleSceneBase::onUpdate(double elapsed) {
  this->elapsed = elapsed;

  camera.Update((float)elapsed);
  background->Update((float)elapsed);

  backdropShader.setUniform("opacity", (float)backdropOpacity);

  counterRevealAnim.Update((float)elapsed, counterReveal.getSprite());
  comboInfoTimer.update(sf::seconds(static_cast<float>(elapsed)));
  multiDeleteTimer.update(sf::seconds(static_cast<float>(elapsed)));
  battleTimer.update(sf::seconds(static_cast<float>(elapsed)));

  switch (backdropMode) {
  case backdrop::fadein:
    backdropOpacity = std::fmin(backdropMaxOpacity, backdropOpacity + (backdropFadeIncrements * elapsed));
    break;
  case backdrop::fadeout:
    backdropOpacity = std::fmax(0.0, backdropOpacity - (backdropFadeIncrements * elapsed));
    if (backdropOpacity == 0.0) {
      backdropAffectBG = false; // reset this effect
    }
    break;
  }

  // Register and eject any applicable components
  ProcessNewestComponents();

  if (!IsPlayerDeleted()) {
    cardUI->OnUpdate((float)elapsed);
  }

  cardCustGUI.Update((float)elapsed);

  newMobSize = mob? mob->GetMobCount() : 0;

  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)) {
    Quit(FadeOut::white);
    Audio().StopStream();
  }

  // State update
  if(!current) return;

  current->onUpdate(elapsed);

  // Update components
  for (auto c : components) {
    if (c->Lifetime() == Component::lifetimes::ui && battleTimer.getElapsed().asMilliseconds() > 0) {
      c->Update((float)elapsed);
    }
    else if (c->Lifetime() == Component::lifetimes::battlestep) {
      // If the mob isn't cleared, only update when the battle-step timer is going
      // Otherwise, feel free to update as the battle is over (mob is cleared)
      bool updateBattleSteps = !mob->IsCleared() && !battleTimer.isPaused();
      updateBattleSteps = updateBattleSteps || mob->IsCleared();
      if (updateBattleSteps) {
        c->Update((float)elapsed);
      }
    }
  }

  // Track combo deletes
  if (lastMobSize != newMobSize && !isPlayerDeleted) {
    int counter = lastMobSize - newMobSize;
    if (multiDeleteTimer.getElapsed() <= sf::seconds(COMBO_HIT_THRESHOLD_SECONDS)) {
      comboDeleteCounter += counter;

      if (comboDeleteCounter == 2) {
        didDoubleDelete = true;
        comboInfoTimer.reset();
      }
      else if (comboDeleteCounter > 2) {
        didTripleDelete = true;
        comboInfoTimer.reset();
      }
    }
    else if (multiDeleteTimer.getElapsed() > sf::seconds(COMBO_HIT_THRESHOLD_SECONDS)) {
      comboDeleteCounter = counter;
    }
  }

  if (lastMobSize != newMobSize) {
    // prepare for another enemy deletion
    multiDeleteTimer.reset();
  }

  lastMobSize = newMobSize;

  for (auto iter = nodeToEdges.begin(); iter != nodeToEdges.end(); iter++) {
    if (iter->first == current) {
      if (iter->second->when()) {
        auto temp = iter->second->b;
        this->last = current;
        this->next = temp;
        current->onEnd(this->next);
        current = temp;
        current->onStart(this->last);
        current->onUpdate(0); // frame-perfect state visuals
        break;
      }
    }
  }

  if (player) {
    battleResults.playerHealth = player->GetHealth();
  }
}

void BattleSceneBase::onDraw(sf::RenderTexture& surface) {
  int tint = static_cast<int>((1.0f - backdropOpacity) * 255);

  if (!backdropAffectBG) {
    tint = 255;
  }

  background->setColor(sf::Color(tint, tint, tint, 255));

  surface.draw(*background);

  auto uis = std::vector<UIComponent*>();

  auto allTiles = field->FindTiles([](Battle::Tile* tile) { return true; });
  auto viewOffset = getController().CameraViewOffset(camera);

  for (Battle::Tile* tile : allTiles) {
    if (tile->IsEdgeTile()) continue;

    if (tile->IsHighlighted() && !this->IsCleared()) {
      tile->SetShader(&yellowShader);
    }
    else {
      tile->RevokeShader();
    }

    tile->move(viewOffset);
    tile->setColor(sf::Color(tint, tint, tint, 255));
    surface.draw(*tile);
    tile->setColor(sf::Color::White);
    tile->move(-viewOffset);
  }

  for (Battle::Tile* tile : allTiles) {
    auto allEntities = tile->FindEntities([](Entity* ent) { return true; });

    for (Entity* ent : allEntities) {
      auto uic = ent->GetComponentsDerivedFrom<UIComponent>();
      uis.insert(uis.begin(), uic.begin(), uic.end());
    }

    auto nodes = std::vector<SceneNode*>();
    nodes.insert(nodes.end(), allEntities.begin(), allEntities.end());
    //nodes.insert(nodes.end(), scenenodes.begin(), scenenodes.end());
    std::sort(nodes.begin(), nodes.end(), [](SceneNode* A, SceneNode* B) { return A->GetLayer() > B->GetLayer(); });

    for (SceneNode* node : nodes) {
      node->move(viewOffset);
      surface.draw(*node);
      node->move(-viewOffset);
    }
  }

  // draw ui on top
  for (UIComponent* ui : uis) {
    if (ui->DrawOnUIPass()) {
      ui->move(viewOffset);
      surface.draw(*ui);
      ui->move(-viewOffset);
    }
  }

  // draw extra card action graphics
  std::vector<Character*> allCharacters;
  field->FindEntities([&allCharacters](Entity* e) mutable {
    if (auto* character = dynamic_cast<Character*>(e)) {
      allCharacters.push_back(character);
    }

    return false;
  });

  for (Character* c : allCharacters) {
    auto actionList = c->AsyncActionList();
    auto currAction = c->CurrentCardAction();

    for (auto action : actionList) {
      surface.draw(*action);
    }

    if (currAction) {
      surface.draw(*currAction);
    }
  }

  // Draw whatever extra state stuff we want to have
  if (current) current->onDraw(surface);
}

void BattleSceneBase::onEnd()
{
  if (this->onEndCallback) {
    this->onEndCallback(battleResults);
  }
}

bool BattleSceneBase::IsPlayerDeleted() const
{
  return isPlayerDeleted;
}

Player* BattleSceneBase::GetPlayer()
{
  return player;
}

Field* BattleSceneBase::GetField()
{
  return field;
}

const Field* BattleSceneBase::GetField() const
{
  return field;
}

CardSelectionCust& BattleSceneBase::GetCardSelectWidget()
{
  return cardCustGUI;
}

PlayerSelectedCardsUI& BattleSceneBase::GetSelectedCardsUI() {
  return *cardUI;
}

PlayerEmotionUI& BattleSceneBase::GetEmotionWindow()
{
  return *emotionUI;
}

Camera& BattleSceneBase::GetCamera()
{
  return camera;
}

PA& BattleSceneBase::GetPA()
{
  return programAdvance;
}

BattleResults& BattleSceneBase::BattleResultsObj()
{
  return battleResults;
}

void BattleSceneBase::StartBattleStepTimer()
{
  battleTimer.start();
}

void BattleSceneBase::StopBattleStepTimer()
{
  battleTimer.pause();
}

void BattleSceneBase::BroadcastBattleStart()
{
  field->RequestBattleStart();
}

void BattleSceneBase::BroadcastBattleStop()
{
  field->RequestBattleStop();
}

const sf::Time BattleSceneBase::GetElapsedBattleTime() {
  return battleTimer.getElapsed();
}

const bool BattleSceneBase::FadeInBackdrop(double amount, double to, bool affectBackground)
{
  backdropMode = backdrop::fadein;
  backdropFadeIncrements = amount;
  backdropMaxOpacity = to;
  backdropAffectBG = affectBackground;

  return (backdropOpacity >= to);
}

const bool BattleSceneBase::FadeOutBackdrop(double amount)
{
  backdropMode = backdrop::fadeout;
  backdropFadeIncrements = amount;

  return (backdropOpacity == 0.0);
}

std::vector<std::reference_wrapper<const Character>> BattleSceneBase::MobList()
{
  std::vector<std::reference_wrapper<const Character>> mobList;

  if (mob) {
    for (int i = 0; i < mob->GetMobCount(); i++) {
      mobList.push_back(mob->GetMobAt(i));
    }
  }

  return mobList;
}

void BattleSceneBase::Quit(const FadeOut& mode) {
  if(quitting) return; 

  // end the current state
  if(current) {
    current->onEnd();
    current = nullptr;
  }

  // Depending on the mode, use Swoosh's 
  // activity controller to fadeout with the right
  // visual appearance
  if(mode == FadeOut::white) {
    getController().pop<segue<WhiteWashFade>>();
  } else if(mode == FadeOut::black) {
    getController().pop<segue<BlackWashFade>>();
  }
  else {
    // mode == FadeOut::pixelate
    getController().pop<segue<PixelateBlackWashFade>>();
  }

  quitting = true;
}


// what to do if we inject a UIComponent, add it to the update and topmost scenenode stack
void BattleSceneBase::Inject(MobHealthUI& other)
{
  other.scene = this;
  components.push_back(&other);
  scenenodes.push_back(&other);
}

void BattleSceneBase::Inject(SelectedCardsUI& cardUI)
{
  this->SubscribeToCardEvents(cardUI);
}

// Default case: no special injection found for the type, just add it to our update loop
void BattleSceneBase::Inject(Component* other)
{
  assert(other && "Component injected was nullptr");

  SceneNode* node = dynamic_cast<SceneNode*>(other);
  if (node) { scenenodes.push_back(node); }

  other->scene = this;
  components.push_back(other);
}

void BattleSceneBase::Eject(Component::ID_t ID)
{
  auto iter = std::find_if(components.begin(), components.end(), 
    [ID](Component* in) { return in->GetID() == ID; }
  );

  if (iter != components.end()) {
    Component* component = *iter;
    // TODO: dynamic casting could be entirely avoided by hashing IDs
    auto iter2 = std::find_if(scenenodes.begin(), scenenodes.end(), 
      [component](SceneNode* in) { 
        return in == dynamic_cast<SceneNode*>(component);
      }
    );

    if (iter2 != scenenodes.end()) {
      scenenodes.erase(iter2);
    }

    components.erase(iter);
  }
}

const bool BattleSceneBase::IsCleared()
{
  return mob? mob->IsCleared() : true;
}

void BattleSceneBase::Link(StateNode& a, StateNode& b, ChangeCondition when) {

  nodeToEdges.insert(std::make_pair(&(a.state), new Edge(&(a.state), &(b.state), when)));
}

void BattleSceneBase::ProcessNewestComponents()
{
  // effectively returns all of them
  auto entities = field->FindEntities([](Entity* e) { return true; });

  for (auto e : entities) {
    if (e->components.size() > 0) {
      // update the ledger
      // this step proceeds the lastComponentID update
      auto latestID = e->components[0]->GetID();

      if (e->lastComponentID < latestID) {
        //std::cout << "latestID: " << latestID << " lastComponentID: " << e->lastComponentID << "\n";

        for (auto c : e->components) {
          if (!c) continue;

          // Older components are last in order, we're done
          if (c->GetID() <= e->lastComponentID) break;

          // Local components are not a part of the battle scene and do not get injected
          if (c->Lifetime() != Component::lifetimes::local) {
            c->Inject(*this);
          }
        }

        e->lastComponentID = latestID;
      }
    }
  }

#ifdef __ANDROID__
  SetupTouchControls();
#endif
}

void BattleSceneBase::onLeave() {
#ifdef __ANDROID__
  ShutdownTouchControls();
#endif
}

#ifdef __ANDROID__
void BattleSceneBase::SetupTouchControls() {
  /* Android touch areas*/
  TouchArea& rightSide = TouchArea::create(sf::IntRect(240, 0, 240, 320));

  rightSide.enableExtendedRelease(true);
  releasedB = false;

  rightSide.onTouch([]() {
    INPUTx.VirtualKeyEvent(InputEvent::RELEASED_A);
    });

  rightSide.onRelease([this](sf::Vector2i delta) {
    if (!releasedB) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_A);
    }

    releasedB = false;

    });

  rightSide.onDrag([this](sf::Vector2i delta) {
    if (delta.x < -25 && !releasedB) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_B);
      INPUTx.VirtualKeyEvent(InputEvent::RELEASED_B);
      releasedB = true;
    }
    });

  rightSide.onDefault([this]() {
    releasedB = false;
    });

  TouchArea& custSelectButton = TouchArea::create(sf::IntRect(100, 0, 380, 100));
  custSelectButton.onTouch([]() {
    INPUTx.VirtualKeyEvent(InputEvent::PRESSED_START);
    });
  custSelectButton.onRelease([](sf::Vector2i delta) {
    INPUTx.VirtualKeyEvent(InputEvent::RELEASED_START);
    });

  TouchArea& dpad = TouchArea::create(sf::IntRect(0, 0, 240, 320));
  dpad.enableExtendedRelease(true);
  dpad.onDrag([](sf::Vector2i delta) {
    Logger::Log("dpad delta: " + std::to_string(delta.x) + ", " + std::to_string(delta.y));

    if (delta.x > 30) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_RIGHT);
    }

    if (delta.x < -30) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_LEFT);
    }

    if (delta.y > 30) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_DOWN);
    }

    if (delta.y < -30) {
      INPUTx.VirtualKeyEvent(InputEvent::PRESSED_UP);
    }
    });

  dpad.onRelease([](sf::Vector2i delta) {
    if (delta.x < -30) {
      INPUTx.VirtualKeyEvent(InputEvent::RELEASED_LEFT);
    }
    });
}

void BattleSceneBase::ShutdownTouchControls() {
  TouchArea::free();
}

#endif