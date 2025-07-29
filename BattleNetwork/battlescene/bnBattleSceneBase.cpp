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
#include "../bnFadeInState.h"
#include "../bnRandom.h"

// Combos are counted if more than one enemy is hit within x frames
// The game is clocked to display 60 frames per second
// If x = 20 frames, then we want a combo hit threshold of 20/60 = 0.3 seconds
#define COMBO_HIT_THRESHOLD_FRAMES frames(20)
#define COUNTER_HIT_THRESHOLD_FRAMES frames(60)

using swoosh::types::segue;
using swoosh::Activity;
using swoosh::ActivityController;

BattleSceneBase::BattleSceneBase(ActivityController& controller, BattleSceneBaseProps& props, BattleResultsFunc onEnd) :
  Scene(controller),
  cardActionListener(this->getController().CardPackagePartitioner()),
  localPlayer(props.player),
  programAdvance(props.programAdvance),
  comboDeleteCounter(0),
  totalCounterMoves(0),
  totalCounterDeletions(0),
  customProgress(0),
  customDuration(10),
  whiteShader(Shaders().GetShader(ShaderType::WHITE_FADE)),
  backdropShader(Shaders().GetShader(ShaderType::BLACK_FADE)),
  yellowShader(Shaders().GetShader(ShaderType::YELLOW)),
  heatShader(Shaders().GetShader(ShaderType::SPOT_DISTORTION)),
  iceShader(Shaders().GetShader(ShaderType::SPOT_REFLECTION)),
  customBarShader(Shaders().GetShader(ShaderType::CUSTOM_BAR)),
  // cap of 8 cards, 8 cards drawn per turn
  cardCustGUI(CardSelectionCust::Props{ std::move(props.folder), &getController().CardPackagePartitioner().GetPartition(Game::LocalPartition), 8, 8 }),
  mobFont(Font::Style::thick),
  camera(sf::View{ sf::Vector2f(240, 160), sf::Vector2f(480, 320) }),
  onEndCallback(onEnd),
  channel(this)
{
  /*
  Set Scene*/
  field = props.field;
  CharacterDeleteListener::Subscribe(*field);
  CharacterSpawnListener::Subscribe(*field);
  field->SetScene(this); // event emitters during battle needs the active scene
  field->HandleMissingLayout();

  // always have mob data even if it's empty for run-time safety
  // using LoadMob() for teams will erase this dummy and empty mob data
  redTeamMob = new Mob(field);
  blueTeamMob = new Mob(field);

  /*
  Background for scene*/
  background = props.background;

  if (!background) {
    int randBG = SyncedRand() % 8;

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

  // Custom bar graphics
  std::shared_ptr<sf::Texture> customBarTexture = Textures().LoadFromFile(TexturePaths::CUST_GAUGE);
  customBar.setTexture(customBarTexture);
  customBar.setOrigin(customBar.getLocalBounds().width / 2, 0);
  sf::Vector2f customBarPos = sf::Vector2f(240.f, 0.f);
  customBar.setPosition(customBarPos);
  customBar.setScale(2.f, 2.f);

  if (customBarShader) {
    customBarShader->setUniform("texture", sf::Shader::CurrentTexture);
    customBarShader->setUniform("factor", 0);
    customBar.SetShader(customBarShader);
  }

  /*
  Counter "reveal" ring
  */

  counterRevealAnim = Animation(AnimationPaths::MISC_COUNTER_REVEAL);
  counterRevealAnim << "DEFAULT" << Animator::Mode::Loop;

  counterReveal = std::make_shared<SpriteProxyNode>();
  counterReveal->setTexture(Textures().LoadFromFile(TexturePaths::MISC_COUNTER_REVEAL), true);
  counterReveal->EnableParentShader(false);
  counterReveal->SetLayer(-100);

  counterCombatRule = std::make_shared<CounterCombatRule>(this);

  // MOB UI
  mobBackdropSprite = sf::Sprite(*Textures().LoadFromFile(TexturePaths::MOB_NAME_BACKDROP));
  mobEdgeSprite = sf::Sprite(*Textures().LoadFromFile(TexturePaths::MOB_NAME_EDGE));

  mobBackdropSprite.setScale(2.f, 2.f);
  mobEdgeSprite.setScale(2.f, 2.f);

  // SHADERS
  // TODO: Load shaders if supported
  shaderCooldown = 0;

  if (whiteShader) {
    whiteShader->setUniform("texture", sf::Shader::CurrentTexture);
    whiteShader->setUniform("opacity", 0.5f);
    whiteShader->setUniform("texture", sf::Shader::CurrentTexture);
  }

  textureSize = getController().getVirtualWindowSize();

  if (iceShader) {
    iceShader->setUniform("texture", sf::Shader::CurrentTexture);
    iceShader->setUniform("sceneTexture", sf::Shader::CurrentTexture);
    iceShader->setUniform("textureSizeIn", sf::Glsl::Vec2((float)textureSize.x, (float)textureSize.y));
    iceShader->setUniform("shine", 0.2f);
  }

  isSceneInFocus = false;

  comboInfoTimer.start();
  multiDeleteTimer.start();

  setView(sf::Vector2u(480, 320));

  // add the camera to our event bus
  channel.Register(&camera);
}

BattleSceneBase::~BattleSceneBase() {
  // drop the camera from our event bus
  channel.Drop(&camera);

  for (BattleSceneState* statePtr : states) {
    delete statePtr;
  }

  for (auto [statePtr, edgePtr] : nodeToEdges) {
    delete edgePtr;
  }

  for (std::shared_ptr<Component>& c : components) {
    c->scene = nullptr;
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

const int BattleSceneBase::GetCounterCount() const {
  return totalCounterMoves;
}

const bool BattleSceneBase::IsSceneInFocus() const
{
  return isSceneInFocus;
}

const bool BattleSceneBase::IsQuitting() const
{
  return quitting;
}

void BattleSceneBase::OnCounter(Entity& victim, Entity& aggressor)
{
  for (std::shared_ptr<Player> p : GetAllPlayers()) {
    if (&aggressor != p.get()) continue;

    if (p == localPlayer) {
      didCounterHit = true; // This flag allows the counter to display
      comboInfoTimer.reset(); // reset display timer
      totalCounterMoves++;

      if (victim.IsDeleted()) {
        totalCounterDeletions++;
      }
    }

    Audio().Play(AudioType::COUNTER, AudioPriority::highest);

    victim.ToggleCounter(false); // disable counter frame for the victim
    victim.Stun(frames(150));

    PreparePlayerFullSynchro(p);
  }
}

void BattleSceneBase::OnSpawnEvent(std::shared_ptr<Character>& spawned)
{
  if (spawned->GetTeam() == Team::red) {
    redTeamMob->Track(spawned);
  }
  else if(spawned->GetTeam() == Team::blue) {
    blueTeamMob->Track(spawned);
  }
}

void BattleSceneBase::OnDeleteEvent(Character& pending)
{
  // Track if player is being deleted
  if (!isPlayerDeleted && localPlayer.get() == &pending) {
    battleResults.runaway = false;
    isPlayerDeleted = true;
  }

  Character* pendingPtr = &pending;

  // Find any AI using this character as a target and free that pointer  
  field->FindEntities([pendingPtr](std::shared_ptr<Entity>& in) {
    Agent* agent = dynamic_cast<Agent*>(in.get());

    if (agent && agent->GetTarget().get() == pendingPtr) {
      agent->FreeTarget();
    }

    return false;
  });

  Logger::Logf(LogLevel::debug, "Removing %s from battle (ID: %d)", pending.GetName().c_str(), pending.GetID());
  
  bool redTeamClear = false;
  bool blueTeamClear = false;
  if (redTeamMob) {
    redTeamMob->Forget(pending);
    redTeamClear = redTeamMob->IsCleared();
    deletingRedMobs.push_back(pending.GetID());
  }

  if (blueTeamMob) {
    blueTeamMob->Forget(pending);
    blueTeamClear = blueTeamMob->IsCleared();
    deletingBlueMobs.push_back(pending.GetID());
  }

  if (redTeamClear || blueTeamClear) {
    Audio().StopStream();
  }
}

const BattleSceneState* BattleSceneBase::GetCurrentState() const
{
  return current;
}

const int BattleSceneBase::ComboDeleteSize() const 
{
  return comboInfoTimer.elapsed() <= COUNTER_HIT_THRESHOLD_FRAMES ? comboDeleteCounter : 0;
}

const bool BattleSceneBase::Countered() const
{
  return comboInfoTimer.elapsed() <= COUNTER_HIT_THRESHOLD_FRAMES && didCounterHit;
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

const bool BattleSceneBase::IsCustGaugeFull() const
{
  return isGaugeFull;
}

void BattleSceneBase::SetCustomBarProgress(double value)
{
  this->customProgress = value;

  if (customProgress < customDuration) {
    isGaugeFull = false;
  }

  if (customBarShader) {
    customBarShader->setUniform("factor", std::min(1.0f, (float)(customProgress/customDuration)));
  }
}

void BattleSceneBase::SetCustomBarDuration(double maxTimeSeconds)
{
  this->customDuration = maxTimeSeconds;
}

void BattleSceneBase::SubscribeToCardActions(CardActionUsePublisher& publisher)
{
  cardActionListener.Subscribe(publisher);
  this->CardActionUseListener::Subscribe(publisher);
  cardUseSubscriptions.push_back(std::ref(publisher));
}

const std::vector<std::reference_wrapper<CardActionUsePublisher>>& BattleSceneBase::GetCardActionSubscriptions() const
{
  return cardUseSubscriptions;
}

TrackedFormData& BattleSceneBase::GetPlayerFormData(const std::shared_ptr<Player>& player)
{
  static TrackedFormData dummy;
  dummy = TrackedFormData();

  auto iter = allPlayerFormsHash.find(player.get());

  if (iter != allPlayerFormsHash.end()) {
    return iter->second;
  }

  return dummy;
}

Team& BattleSceneBase::GetPlayerTeamData(const std::shared_ptr<Player>& player)
{
  static Team dummy;
  dummy = Team::unknown;

  auto iter = allPlayerTeamHash.find(player.get());

  if (iter != allPlayerTeamHash.end()) {
    return iter->second;
  }

  return dummy;
}

std::shared_ptr<Player> BattleSceneBase::GetPlayerFromEntityID(Entity::ID_t ID)
{
  for (std::shared_ptr<Player> player : GetAllPlayers()) {
    if (player->GetID() == ID) return player;
  }

  return nullptr;
}

void BattleSceneBase::OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp)
{
  HandleCounterLoss(*action->GetActor(), true);
}

sf::Vector2f BattleSceneBase::PerspectiveOffset(const sf::Vector2f& pos)
{
  if (perspectiveFlip) {
    return sf::Vector2f((240.f - pos.x)*2.f, 0.f);
  }

  return sf::Vector2f();
}

sf::Vector2f BattleSceneBase::PerspectiveOrigin(const sf::Vector2f& origin, const sf::FloatRect& size)
{
  if (perspectiveFlip) {
    float rectW = size.width;
    float canX = (origin.x / rectW);
    float originF = 1.f - canX;
    float screenX = rectW * originF;
    return sf::Vector2f(screenX, origin.y);
  }

  return origin;
}

void BattleSceneBase::SpawnLocalPlayer(int x, int y)
{
  if (hasPlayerSpawned) return;
  hasPlayerSpawned = true;
  Team team = field->GetAt(x, y)->GetTeam();

  localPlayer->Init();
  localPlayer->ChangeState<PlayerIdleState>();
  localPlayer->SetTeam(team);
  field->AddEntity(localPlayer, x, y);

  // Player UI
  cardUI = localPlayer->CreateComponent<PlayerSelectedCardsUI>(localPlayer, &getController().CardPackagePartitioner());
  this->SubscribeToCardActions(*localPlayer);
  this->SubscribeToCardActions(*cardUI);

  auto healthUI = localPlayer->CreateComponent<PlayerHealthUIComponent>(localPlayer);
  healthUI->setScale(2.f, 2.f); // TODO: this should be upscaled by cardCustGUI transforms... why is it not?

  cardCustGUI.AddNode(healthUI);

  // Player Emotion
  this->emotionUI = localPlayer->CreateComponent<PlayerEmotionUI>(localPlayer);
  this->emotionUI->setScale(2.f, 2.f); // TODO: this should be upscaled by cardCustGUI transforms... why is it not?
  this->emotionUI->setPosition(4.f, 35.f);
  cardCustGUI.AddNode(emotionUI);

  emotionUI->Subscribe(cardCustGUI);

  // Load forms
  cardCustGUI.SetPlayerFormOptions(localPlayer->GetForms());

  // track forms
  allPlayerFormsHash[localPlayer.get()] = {}; // use default form data values
  allPlayerTeamHash[localPlayer.get()] = team;

  HitListener::Subscribe(*localPlayer);

  PreparePlayerFullSynchro(localPlayer);
}

void BattleSceneBase::SpawnOtherPlayer(std::shared_ptr<Player> player, int x, int y)
{
  if (!TrackOtherPlayer(player)) return;

  Team team = field->GetAt(x, y)->GetTeam();

  player->Init();
  player->ChangeState<PlayerIdleState>();  
  player->SetTeam(team);
  field->AddEntity(player, x, y);

  // Other Player UI
  std::shared_ptr<PlayerSelectedCardsUI> cardUI = player->CreateComponent<PlayerSelectedCardsUI>(player, &getController().CardPackagePartitioner());
  cardUI->Hide();
  this->SubscribeToCardActions(*player);
  SubscribeToCardActions(*cardUI);

  std::shared_ptr<MobHealthUI> healthUI = player->CreateComponent<MobHealthUI>(player);

  // track forms
  allPlayerFormsHash[player.get()] = {}; // use default form data values
  allPlayerTeamHash[player.get()] = team;

  HitListener::Subscribe(*player);

  PreparePlayerFullSynchro(player);
}

void BattleSceneBase::LoadRedTeamMob(Mob& mob)
{
  if (redTeamMob) {
    delete redTeamMob;
  }

  redTeamMob = &mob;
}

void BattleSceneBase::LoadBlueTeamMob(Mob& mob)
{
  if (blueTeamMob) {
    delete blueTeamMob;
  }

  blueTeamMob = &mob;
}

void BattleSceneBase::HandleCounterLoss(Entity& subject, bool playsound)
{
  if (&subject == localPlayer.get()) {
    if (field->DoesRevealCounterFrames()) {
      localPlayer->RemoveNode(counterReveal);
      localPlayer->RemoveDefenseRule(counterCombatRule);
      localPlayer->SetEmotion(Emotion::normal);
      field->RevealCounterFrames(false);

      playsound ? Audio().Play(AudioType::COUNTER_BONUS, AudioPriority::highest) : 0;
    }
    cardUI->SetMultiplier(1);
  }
}

void BattleSceneBase::FilterSupportCards(const std::shared_ptr<Player>& player, std::vector<Battle::Card>& cards) {
  CardPackagePartitioner& partitions = getController().CardPackagePartitioner();

  for (size_t i = 0; i < cards.size(); i++) {
    std::string uuid = cards[i].GetUUID();
    stx::result_t<PackageAddress> maybe_addr = PackageAddress::FromStr(uuid);

    if(!maybe_addr.is_error()) {
      PackageAddress addr = maybe_addr.value();

      if (partitions.HasNamespace(addr.namespaceId)) {
        CardPackageManager& cardPackageManager = partitions.GetPartition(addr.namespaceId);

        // booster cards do not modify other booster cards
        if (cardPackageManager.HasPackage(addr.packageId)) {
          AdjacentCards adjCards;

          if (i > 0 && cards[i - 1u].CanBoost()) {
            adjCards.hasCardToLeft = true;
            adjCards.leftCard = &cards[i - 1u].props;
          }

          if (i < cards.size() - 1 && cards[i + 1u].CanBoost()) {
            adjCards.hasCardToRight = true;
            adjCards.rightCard = &cards[i + 1u].props;
          }

          CardMeta& meta = cardPackageManager.FindPackageByID(addr.packageId);

          if (meta.filterHandStep) {
            meta.filterHandStep(cards[i].props, adjCards);
          }

          size_t this_card = i;
          /* 
            Whether or not to do another loop on this index.
            True when left, right, or this card were deleted.

            By setting true on left and right delete, the filter 
            step will run for this card again so that it can run 
            with the new adjacent cards.

            By setting true when deleting itself, the new card 
            coming into this index won't be skipped.
          */
          bool check_again = false;

          if (adjCards.deleteLeft) {
            cards.erase(cards.begin() + this_card - 1u);
            this_card--;
            check_again = true;
          }

          if (adjCards.deleteRight) {
            cards.erase(cards.begin() + this_card + 1u);
            // This card index hasn't changed

            check_again = true;
          }

          if (adjCards.deleteThisCard) {
            cards.erase(cards.begin() + this_card);
            check_again = true;
          }

          if (check_again) {
            i--;
          }
        }
      }
    }
  }
}

#ifdef __ANDROID__
void BattleSceneBase::SetupTouchControls() {

}

void BattleSceneBase::ShutdownTouchControls() {

}
#endif

void BattleSceneBase::DrawCustGauage(sf::RenderTexture& surface)
{
  if (!(redTeamMob && redTeamMob->IsCleared()) || !(blueTeamMob && blueTeamMob->IsCleared())) {
    surface.draw(customBar);
  }
}

void BattleSceneBase::StartStateGraph(StateNode& start) {
  for (std::shared_ptr<Player> p : GetAllPlayers()) {
    if (!localPlayer->Teammate(p->GetTeam())) {
      Logger::Logf(LogLevel::debug, "Mob is tracking other player %s", p->GetName().c_str());
    }

    if (p->GetTeam() == Team::red) {
      if(redTeamMob)
        redTeamMob->Track(p);
    }
    else {
      if(blueTeamMob)
        blueTeamMob->Track(p);
    }
  }

  PerspectiveFlip(localPlayer->GetTeam() == Team::blue);

  field->Update(0);

  // set the current state ptr and kick-off
  current = &start.state;
  current->onStart();
}

void BattleSceneBase::onStart()
{
  isSceneInFocus = true;

  // Stream battle music
  if (blueTeamMob && blueTeamMob->HasCustomMusicPath()) {
    const std::array<long long, 2> points = blueTeamMob->GetLoopPoints();
    Audio().Stream(blueTeamMob->GetCustomMusicPath(), true, points[0], points[1]);
  }
  else {
    if (blueTeamMob == nullptr || !blueTeamMob->IsBoss()) {
      Audio().Stream("resources/loops/loop_battle.ogg", true);
    }
    else {
      Audio().Stream("resources/loops/loop_boss_battle.ogg", true);
    }
  }
}

void BattleSceneBase::onUpdate(double elapsed) {
  this->elapsed = elapsed;

  if (getController().CommandLineValue<bool>("debug")) {
    if (Input().Has(InputEvents::pressed_map)) {
      perspectiveFlip = !perspectiveFlip;
    }
  }

  background->Update((float)elapsed);

  if (Input().GetAnyKey() == sf::Keyboard::Escape && this->IsSceneInFocus()) {
    BroadcastBattleStop();
    Quit(FadeOut::white);
    Audio().StopStream();
  }

  // State update
  if (!current) return;
  
  if (skipFrame) {
    skipFrame = false;
    return;
  }

  IncrementFrame();

  camera.Update((float)elapsed);

  if (backdropShader) {
    backdropShader->setUniform("opacity", (float)backdropOpacity);
  }

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

  newRedTeamMobSize = redTeamMob ? redTeamMob->GetMobCount() : 0;
  newBlueTeamMobSize = blueTeamMob ? blueTeamMob->GetMobCount() : 0;

  current->onUpdate(elapsed);

  if (customProgress / customDuration >= 1.0 && !isGaugeFull) {
    isGaugeFull = true;
    Audio().Play(AudioType::CUSTOM_BAR_FULL);
  }

  std::vector<std::shared_ptr<Component>> componentsCopy = components;

  // Update components
  for (std::shared_ptr<Component>& c : componentsCopy) {
    if (c->Lifetime() == Component::lifetimes::ui) {
      c->Update((float)elapsed);
    }
    else if (c->Lifetime() == Component::lifetimes::battlestep) {
      // If the mob isn't cleared, only update when the battle-step timer is going
      // Otherwise, feel free to update as the battle is over (mob is cleared)
      bool isCleared = (redTeamMob && redTeamMob->IsCleared()) || (blueTeamMob && blueTeamMob->IsCleared());
      bool updateBattleSteps = !isCleared && !battleTimer.is_paused();
      updateBattleSteps = updateBattleSteps || isCleared;
      if (updateBattleSteps) {
        c->Update((float)elapsed);
      }
    }
  }

  counterRevealAnim.Update((float)elapsed, counterReveal->getSprite());

  if (!battleTimer.is_paused()) {
    comboInfoTimer.tick();
    multiDeleteTimer.tick();

    if (comboInfoTimer.elapsed() > COUNTER_HIT_THRESHOLD_FRAMES) {
      didCounterHit = didDoubleDelete = didTripleDelete = false;
    }
  }

  battleTimer.tick();

  // Track combo deletes
  const int& lastMobSize = localPlayer->GetTeam() == Team::red ? blueTeamMob->GetMobCount() : redTeamMob->GetMobCount();
  int& newMobSize = localPlayer->GetTeam() == Team::red ? newBlueTeamMobSize : newRedTeamMobSize;

  if (lastMobSize != newMobSize && !isPlayerDeleted) {
    int counter = newMobSize - lastMobSize; // this frame's mob size - mob size after 1 frame of update = combo delete count
    if (multiDeleteTimer.elapsed() <= COMBO_HIT_THRESHOLD_FRAMES) {
      comboDeleteCounter += counter;

      if (comboDeleteCounter == 2) {
        didDoubleDelete = true;
      }
      else if (comboDeleteCounter > 2) {
        didTripleDelete = true;
      }
    }
    else if (multiDeleteTimer.elapsed() > COMBO_HIT_THRESHOLD_FRAMES) {
      comboDeleteCounter = counter;
    }

    comboInfoTimer.reset();
  }

  if (lastMobSize != newMobSize) {
    // prepare for another enemy deletion
    multiDeleteTimer.reset();
  }

  lastRedTeamMobSize = newRedTeamMobSize;
  lastBlueTeamMobSize = newBlueTeamMobSize;

  for (auto iter = nodeToEdges.begin(); iter != nodeToEdges.end(); iter++) {
    if (iter->first == current) {
      if (iter->second->when()) {
        BattleSceneState* temp = iter->second->b;
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

  // cleanup trackers for ex-mob enemies when they are fully removed from the field
  // 
  // red team mobs
  for (auto iter = deletingRedMobs.begin(); iter != deletingRedMobs.end(); /*skip*/) {
    if (!field->GetEntity(*iter)) {
      iter = deletingRedMobs.erase(iter);
      continue;
    }

    iter++;
  }

  //
  // blue team mobs
  for (auto iter = deletingBlueMobs.begin(); iter != deletingBlueMobs.end(); /*skip*/) {
    if (!field->GetEntity(*iter)) {
      iter = deletingBlueMobs.erase(iter);
      continue;
    }

    iter++;
  }

  if (localPlayer) {
    battleResults.playerHealth = localPlayer->GetHealth();
  }

  // custom bar continues to animate when it is already full
  if (isGaugeFull) {
    customFullAnimDelta += elapsed/customDuration;
    customBarShader->setUniform("factor", (float)(1.0 + customFullAnimDelta));
  }

  // Find and handle traitors
  for (std::shared_ptr<Player> p : GetAllPlayers()) {
    Team team = p->GetTeam();
    Team& oldTeam = GetPlayerTeamData(p);

    if (oldTeam != team) {
      // inform the mob trackers
      Mob& oldMob = (oldTeam == Team::red)? *redTeamMob : *blueTeamMob;
      Mob& newMob = (team == Team::red)? *redTeamMob : *blueTeamMob;

      oldMob.Forget(*p);
      newMob.Track(p);

      // flip perspective so the traitor can see correctly
      if (p == GetLocalPlayer()) {
        PerspectiveFlip(team == Team::blue);
      }

      // update our record
      oldTeam = team;
    }
  }
}

void BattleSceneBase::onDraw(sf::RenderTexture& surface) {
  int tint = static_cast<int>((1.0f - backdropOpacity) * 255);

  if (!backdropAffectBG) {
    tint = 255;
  }

  background->SetOpacity(1.0f - (float)backdropOpacity);

  surface.draw(*background);

  auto uis = std::vector<std::shared_ptr<UIComponent>>();

  std::vector<Battle::Tile*> allTiles = field->FindTiles([](Battle::Tile* tile) { return true; });
  sf::Vector2f viewOffset = getController().CameraViewOffset(camera);

  for (Battle::Tile* tile : allTiles) {
    if (tile->IsEdgeTile()) continue;

    bool yellowBlock = false;
    bool isCleared = (redTeamMob && redTeamMob->IsCleared()) || (blueTeamMob && blueTeamMob->IsCleared());
    if (tile->IsHighlighted() && !isCleared) {
      if (!yellowShader) {
        yellowBlock = true;
      }
      else {
        tile->SetShader(yellowShader);
      }
    }
    else {
      tile->RevokeShader();
      tile->setColor(sf::Color::White);
    }

    sf::Vector2f flipOffset = PerspectiveOffset(tile->getPosition());
    tile->PerspectiveFlip(perspectiveFlip);
    tile->move(viewOffset + flipOffset);
    tile->setColor(sf::Color(tint, tint, tint, 255));
    surface.draw(*tile);
    tile->setColor(sf::Color::White);

    if (yellowBlock) {
      sf::RectangleShape block;
      block.setSize({40, 30});
      block.setScale(2.f, 2.f);
      block.setOrigin(20, 15);
      block.setFillColor(sf::Color::Yellow);
      block.setPosition(tile->getPosition());
      surface.draw(block);
    }

    tile->move(-(viewOffset+flipOffset));
    tile->PerspectiveFlip(false);
  }

  std::vector<Entity*> allEntities;

  for (Battle::Tile* tile : allTiles) {
    std::vector<Entity*> tileEntities;
    tile->FindEntities([&tileEntities, &allEntities](std::shared_ptr<Entity>& ent) {
      tileEntities.push_back(ent.get());
      allEntities.push_back(ent.get());
      return false;
    });

    std::sort(tileEntities.begin(), tileEntities.end(), [](Entity* A, Entity* B) { return A->GetLayer() > B->GetLayer(); });

    for (Entity* node : tileEntities) {
      sf::Vector2f offset = viewOffset + sf::Vector2f(0, -node->GetElevation());
      sf::Vector2f flipOffset = PerspectiveOffset(node->getPosition());
      node->move(offset+flipOffset);

      sf::Vector2f ogScale = node->getScale();

      if (perspectiveFlip && !node->neverFlip) {
        node->setScale(-ogScale.x, ogScale.y);
      }

      node->ShiftShadow();

      // TODO: having the field use hashes to identify entity types purely by ID would be faster
      //       and remove all dynamic casting in the engine...
      bool isSpell = (dynamic_cast<Spell*>(node) != nullptr);
      if (isSpell || localPlayer->Teammate(node->GetTeam()) || !localPlayer->IsBlind()) {
        surface.draw(*node);
      }

      if (perspectiveFlip) {
        node->setScale(ogScale.x, ogScale.y);
      }

      node->move(-(offset+flipOffset));
    }
  }

  std::vector<Character*> allCharacters;

  // draw ui on top
  for (Entity* ent : allEntities) {
    // skip this entity's UI components if player is blinded
    if (!(localPlayer->Teammate(ent->GetTeam()) || !localPlayer->IsBlind())) continue;

    std::vector<std::shared_ptr<UIComponent>> uis = ent->GetComponentsDerivedFrom<UIComponent>();
    sf::Vector2f flipOffset = PerspectiveOffset(ent->getPosition());
    for (std::shared_ptr<UIComponent>& ui : uis) {
      if (ui->DrawOnUIPass()) {
        ui->move(viewOffset + flipOffset);
        surface.draw(*ui);
        ui->move(-(viewOffset + flipOffset));
      }
    }

    // collect characters while drawing ui
    if (Character* character = dynamic_cast<Character*>(ent)) {
      allCharacters.push_back(character);
    }
  }

  // draw extra card action graphics
  for (Character* c : allCharacters) {
    const std::vector<std::shared_ptr<CardAction>> actionList = c->AsyncActionList();
    std::shared_ptr<CardAction> currAction = c->CurrentCardAction();

    for (const std::shared_ptr<CardAction>& action : actionList) {
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
  if (onEndCallback) {
    onEndCallback(battleResults);
  }
}

bool BattleSceneBase::TrackOtherPlayer(std::shared_ptr<Player>& other) {
  if (other == localPlayer) return false; // prevent tracking local player as "other" players

  auto iter = std::find(otherPlayers.begin(), otherPlayers.end(), other);

  if (iter == otherPlayers.end()) {
    otherPlayers.push_back(other);
    return true;
  }

  return false;
}

void BattleSceneBase::UntrackOtherPlayer(std::shared_ptr<Player>& other) {
  auto iter = std::find(otherPlayers.begin(), otherPlayers.end(), other);
  auto iter2 = allPlayerFormsHash.find(other.get());
  auto iter3 = allPlayerTeamHash.find(other.get());
  if (iter != otherPlayers.end()) {
    otherPlayers.erase(iter);
    allPlayerFormsHash.erase(iter2);
    allPlayerTeamHash.erase(iter3);
  }
}

void BattleSceneBase::UntrackMobCharacter(std::shared_ptr<Character>& character)
{
  redTeamMob->Forget(*character.get());
  blueTeamMob->Forget(*character.get());
}

void BattleSceneBase::DrawWithPerspective(sf::Shape& shape, sf::RenderTarget& surf)
{
  sf::Vector2f position = shape.getPosition();
  sf::Vector2f origin = shape.getOrigin();
  sf::Vector2f offset = PerspectiveOffset(position);
  sf::Vector2f originNew = PerspectiveOrigin(shape.getOrigin(), shape.getLocalBounds());

  shape.setPosition(shape.getPosition() + offset);
  shape.setOrigin(originNew);

  surf.draw(shape);

  shape.setPosition(position);
  shape.setOrigin(origin);
}

void BattleSceneBase::PreparePlayerFullSynchro(const std::shared_ptr<Player>& player) {
  if (player->IsInForm() == true || player->GetEmotion() == Emotion::evil) return;

  if (player == localPlayer) {
    field->RevealCounterFrames(true);
  }

  // node positions are relative to the parent node's origin
  sf::FloatRect bounds = player->getLocalBounds();
  counterReveal->setPosition(0, -bounds.height / 4.0f);
  player->AddNode(counterReveal);

  std::shared_ptr<PlayerSelectedCardsUI> cardUI = player->GetFirstComponent<PlayerSelectedCardsUI>();

  if (cardUI) {
    cardUI->SetMultiplier(2);
  }

  player->SetEmotion(Emotion::full_synchro);

  // when players get hit by impact, battle scene takes back counter blessings
  player->AddDefenseRule(counterCombatRule);
}

void BattleSceneBase::DrawWithPerspective(sf::Sprite& sprite, sf::RenderTarget& surf)
{
  sf::Vector2f position = sprite.getPosition();
  sf::Vector2f origin = sprite.getOrigin();
  sf::Vector2f offset = PerspectiveOffset(position);
  sf::Vector2f originNew = PerspectiveOrigin(sprite.getOrigin(), sprite.getLocalBounds());

  sprite.setPosition(sprite.getPosition() + offset);
  sprite.setOrigin(originNew);

  surf.draw(sprite);

  sprite.setPosition(position);
  sprite.setOrigin(origin);
}

void BattleSceneBase::DrawWithPerspective(Text& text, sf::RenderTarget& surf)
{
  sf::Vector2f position = text.getPosition();
  sf::Vector2f origin = text.getOrigin();
  sf::Vector2f offset = PerspectiveOffset(position);
  sf::Vector2f originNew = PerspectiveOrigin(text.getOrigin(), text.GetLocalBounds());

  text.setPosition(text.getPosition() + offset);
  text.setOrigin(originNew);

  surf.draw(text);

  text.setPosition(position);
  text.setOrigin(origin);  
}

void BattleSceneBase::PerspectiveFlip(bool flipped)
{
  perspectiveFlip = flipped;
}

bool BattleSceneBase::IsPlayerDeleted() const
{
  return isPlayerDeleted;
}

std::shared_ptr<Player> BattleSceneBase::GetLocalPlayer() {
  return localPlayer;
}

const std::shared_ptr<Player> BattleSceneBase::GetLocalPlayer() const {
  return localPlayer;
}

std::vector<std::shared_ptr<Player>> BattleSceneBase::GetOtherPlayers()
{
  return otherPlayers;
}

std::vector<std::shared_ptr<Player>> BattleSceneBase::GetAllPlayers()
{
  std::vector<std::shared_ptr<Player>> result = otherPlayers;
  result.insert(result.begin(), localPlayer);
  return result;
}


std::shared_ptr<Field> BattleSceneBase::GetField()
{
  return field;
}

const std::shared_ptr<Field> BattleSceneBase::GetField() const
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

const int BattleSceneBase::GetTurnCount()
{
  return turn;
}

const int BattleSceneBase::GetRoundCount()
{
  return round;
}

const frame_time_t BattleSceneBase::FrameNumber() const
{
  return frameNumber;
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

void BattleSceneBase::IncrementTurnCount()
{
  turn++;
}

void BattleSceneBase::IncrementRoundCount()
{
  round++;
}

void BattleSceneBase::SkipFrame()
{
  skipFrame = true;
}

void BattleSceneBase::IncrementFrame()
{
  frameNumber++;
}

const frame_time_t BattleSceneBase::GetElapsedBattleFrames() {
  return battleTimer.elapsed();
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

std::vector<std::reference_wrapper<const Character>> BattleSceneBase::RedTeamMobList()
{
  std::vector<std::reference_wrapper<const Character>> mobList;

  if (redTeamMob) {
    for (int i = 0; i < redTeamMob->GetMobCount(); i++) {
      mobList.push_back(redTeamMob->GetMobAt(i));
    }
  }

  return mobList;
}

std::vector<std::reference_wrapper<const Character>> BattleSceneBase::BlueTeamMobList()
{
  std::vector<std::reference_wrapper<const Character>> mobList;

  if (blueTeamMob) {
    for (int i = 0; i < blueTeamMob->GetMobCount(); i++) {
      mobList.push_back(blueTeamMob->GetMobAt(i));
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

  // NOTE: swoosh quirk
  if (getController().getStackSize() == 1) {
    getController().pop();
    quitting = true;
    return;
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
void BattleSceneBase::Inject(std::shared_ptr<MobHealthUI> other)
{
  other->scene = this;
  components.push_back(other);
  scenenodes.push_back(other);
}

void BattleSceneBase::Inject(std::shared_ptr<SelectedCardsUI> cardUI)
{
  SubscribeToCardActions(*cardUI);
}

// Default case: no special injection found for the type, just add it to our update loop
void BattleSceneBase::Inject(std::shared_ptr<Component> other)
{
  assert(other && "Component injected was nullptr");

  std::shared_ptr<SceneNode> node = std::dynamic_pointer_cast<SceneNode>(other);
  if (node) { scenenodes.push_back(node); }

  other->scene = this;
  components.push_back(other);
}

void BattleSceneBase::Eject(Component::ID_t ID)
{
  auto iter = std::find_if(components.begin(), components.end(), 
    [ID](std::shared_ptr<Component>& in) { return in->GetID() == ID; }
  );

  if (iter != components.end()) {
    Component& component = **iter;

    SceneNode* node = dynamic_cast<SceneNode*>(&component);
    // TODO: dynamic casting could be entirely avoided by hashing IDs
    auto iter2 = std::find_if(scenenodes.begin(), scenenodes.end(), 
      [node](std::shared_ptr<SceneNode>& in) { 
        return in.get() == node;
      }
    );

    if (iter2 != scenenodes.end()) {
      scenenodes.erase(iter2);
    }

    component.scene = nullptr;
    components.erase(iter);
  }
}

void BattleSceneBase::TrackCharacter(std::shared_ptr<Character> newCharacter)
{
  if (newCharacter->GetTeam() == Team::red) {
    redTeamMob->Track(newCharacter);
  }
  else {
    blueTeamMob->Track(newCharacter);
  }
}

const bool BattleSceneBase::IsRedTeamCleared() const
{
  return redTeamMob? redTeamMob->IsCleared() && deletingRedMobs.empty() : true;
}

const bool BattleSceneBase::IsBlueTeamCleared() const
{
  return blueTeamMob ? blueTeamMob->IsCleared() && deletingBlueMobs.empty() : true;
}

void BattleSceneBase::Link(StateNode& a, StateNode& b, ChangeCondition when) {

  nodeToEdges.insert(std::make_pair(&(a.state), new Edge(&(a.state), &(b.state), when)));
}

void BattleSceneBase::ProcessNewestComponents()
{
  // effectively returns all of them
  std::vector<Entity*> entities;
  field->FindEntities([&entities](std::shared_ptr<Entity>& e) {
    entities.push_back(e.get());
    return false;
  });

  for (Entity* e : entities) {
    if (e->components.size() > 0) {
      // update the ledger
      // this step proceeds the lastComponentID update
      Component::ID_t latestID = e->components[0]->GetID();

      if (e->lastComponentID < latestID) {
        //std::cout << "latestID: " << latestID << " lastComponentID: " << e->lastComponentID << "\n";

        for (std::shared_ptr<Component>& c : e->components) {
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

void BattleSceneBase::FlushLocalPlayerInputQueue()
{
  queuedLocalEvents.clear();
}

std::vector<InputEvent> BattleSceneBase::ProcessLocalPlayerInputQueue(unsigned int lag, bool gatherInput) {
  std::vector<InputEvent> outEvents;

  if (!localPlayer) return outEvents;

  // For all inputs in the queue, reduce their wait time for this new frame
  for (InputEvent& item : queuedLocalEvents) {
    item.wait--;
  }

  
  if (gatherInput) {
    // For all new input events, set the wait time based on the network latency and append
    const auto events_this_frame = Input().StateThisFrame();

    for (auto& [name, state] : events_this_frame) {
     // if (state != InputState::pressed && state != InputState::held) {
     //   let VirtualInputState resolve release
     //   continue;
     // }

      InputEvent copy = InputEvent{ name, state };
      copy.name = name;
      copy.state = InputState::pressed; // VirtualInputState will handle this

      outEvents.push_back(copy);

      // add delay for network
      copy.wait = lag;
      queuedLocalEvents.push_back(copy);
    }
  }
  
  // Drop inputs that are already processed at the end of the last frame
  for (auto iter = queuedLocalEvents.begin(); iter != queuedLocalEvents.end();) {
    if (iter->wait <= 0) {
      localPlayer->InputState().VirtualKeyEvent(*iter);
      iter = queuedLocalEvents.erase(iter);
      continue;
    }

    iter++;
  }

 // localPlayer->InputState().Process();
  return outEvents;
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