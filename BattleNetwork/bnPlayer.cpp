#include "bnPlayer.h"
#include "bnNaviWhiteoutState.h"
#include "bnField.h"
#include "bnBusterCardAction.h"
#include "bnTextureResourceManager.h"
#include "bnAudioResourceManager.h"
#include "bnGame.h"
#include "bnLogger.h"

#include "bnBubbleTrap.h"
#include "bnBubbleState.h"

#define RESOURCE_PATH "resources/navis/megaman/megaman.animation"

Player::Player() :
  state(PLAYER_IDLE),
  chargeEffect(std::make_shared<ChargeEffectSceneNode>(this)),
  AI<Player>(this),
  Character(Rank::_1),
  emotion{Emotion::normal}
{
  ChangeState<PlayerIdleState>();
  
  // The charge component is also a scene node
  // Make sure the charge is in front of this node
  // Otherwise children scene nodes are drawn behind 
  // their parents
  chargeEffect->SetLayer(-2);
  AddNode(chargeEffect);
  chargeEffect->setPosition(0, -20.0f); // translate up -20
  chargeEffect->EnableParentShader(false);

  SetLayer(0);

  setScale(2.0f, 2.0f);

  previous = nullptr;
  playerControllerSlide = false;
  activeForm = nullptr;
  superArmor = std::make_shared<DefenseSuperArmor>();

  auto flinch = [this]() {
    ClearActionQueue();
    Charge(false);

    // At the end of flinch we need to be made actionable if possible
    SetAnimation(recoilAnimHash, [this] { MakeActionable();});
    Audio().Play(AudioType::HURT, AudioPriority::lowest);
  };

  RegisterStatusCallback(Hit::flinch, Callback<void()>{ flinch });

  using namespace std::placeholders;
  auto handler = std::bind(&Player::HandleBusterEvent, this, _1, _2);

  actionQueue.RegisterType<BusterEvent>(ActionTypes::buster, handler);

  // When we have no upcoming actions we should be in IDLE state
  actionQueue.SetIdleCallback([this] {
    if (!IsActionable()) {
      auto finish = [this] {
        MakeActionable();
      };

      animationComponent->OnFinish(finish);
    }
  });
}

void Player::Init() {
  if (HasInit()) return;

  Character::Init();

  animationComponent = CreateComponent<AnimationComponent>(weak_from_this());
  animationComponent->SetPath(RESOURCE_PATH);
  animationComponent->Reload();

  FinishConstructor();
}

Player::~Player() {
  for (PlayerFormMeta* form : forms) {
    delete form;
  }

  if (activeForm) {
    delete activeForm;
  }

  actionQueue.ClearQueue(ActionQueue::CleanupType::clear_and_reset);
}

std::shared_ptr<SyncNode> Player::AddSyncNode(const std::string& point) {
  return syncNodeContainer.AddSyncNode(*this, *animationComponent, point);
}

void Player::RemoveSyncNode(std::shared_ptr<SyncNode> syncNode) {
  syncNodeContainer.RemoveSyncNode(*this, *animationComponent, syncNode);
}

void Player::OnUpdate(double _elapsed) {
  SetColorMode(ColorMode::additive);

  if (!IsInForm()) {
    if (emotion == Emotion::angry) {
      setColor(sf::Color(155, 0, 0, getColor().a));
    }
    else if (emotion == Emotion::full_synchro) {
      setColor(sf::Color(55, 55, 155, getColor().a));
    }
    else if (emotion == Emotion::evil) {
      setColor(sf::Color(155, 100, 255, getColor().a));
    }

    if (emotion == Emotion::evil) {
      SetColorMode(ColorMode::multiply);
    }
  }

  AI<Player>::Update(_elapsed);

  if (activeForm) {
    activeForm->OnUpdate(_elapsed, shared_from_base<Player>());
  }

  //Node updates
  chargeEffect->Update(_elapsed);

  fullyCharged = chargeEffect->IsFullyCharged();
}

void Player::MakeActionable()
{
  animationComponent->CancelCallbacks();

  if (!IsActionable()) {
    animationComponent->SetAnimation("PLAYER_IDLE");
    animationComponent->SetPlaybackMode(Animator::Mode::Loop);
  }
}

bool Player::IsActionable() const
{
  return animationComponent->GetAnimationString() == "PLAYER_IDLE";
}

void Player::Attack() {
  std::shared_ptr<CardAction> action = nullptr;

  // Queue an action for the controller to fire at the right frame
  if (tile) {
    fullyCharged? action = ExecuteChargedBuster() : action = ExecuteBuster(); 
    
    if (action) {
      action->PreventCounters();

      Battle::Card::Properties props = action->GetMetaData();

      if (!fullyCharged) {
        props.timeFreeze = false;
        props.shortname = "Buster";
      }

      action->SetMetaData(props);
      action->SetLockoutGroup(CardAction::LockoutGroup::weapon);
      BusterEvent event = { frames(0), frames(0), false, std::shared_ptr<CardAction>(action) };
      actionQueue.Add(std::move(event), ActionOrder::voluntary, ActionDiscardOp::until_eof);
    }
  }
}

void Player::UseSpecial()
{
  if (tile) {
    if (std::shared_ptr<CardAction> action = ExecuteSpecial()) {
      action->PreventCounters();
      action->SetLockoutGroup(CardAction::LockoutGroup::ability);
      CardEvent event = { std::shared_ptr<CardAction>(action) };
      actionQueue.Add(std::move(event), ActionOrder::voluntary, ActionDiscardOp::until_eof);
    }
  }
}

void Player::HandleBusterEvent(const BusterEvent& event, const ActionQueue::ExecutionType& exec)
{
  Character::HandleCardEvent({ std::shared_ptr<CardAction>(event.action) }, exec);
}

void Player::OnDelete() {
  chargeEffect->Hide();
  actionQueue.ClearQueue(ActionQueue::CleanupType::clear_and_reset);

  // Cleanup child nodes
  // TODO: this should have been cleaned up automatically by card actions...
  // auto ourNodes = GetChildNodesWithTag({ Player::BASE_NODE_TAG, Player::FORM_NODE_TAG });
  std::vector<std::shared_ptr<SceneNode>> allNodes = GetChildNodes();

  for (std::shared_ptr<SceneNode>& node : allNodes) {
    RemoveNode(node);
  }

  std::shared_ptr<AnimationComponent> animationComponent = GetFirstComponent<AnimationComponent>();

  if (animationComponent) {
      animationComponent->CancelCallbacks();
      animationComponent->SetAnimation(PLAYER_HIT);
      animationComponent->Refresh();
  }

  ChangeState<NaviWhiteoutState<Player>>();
}

const float Player::GetHeight() const
{
  return 50.0f;
}

int Player::GetMoveCount() const
{
  return Entity::GetMoveCount();
}

void Player::Charge(bool state)
{
  frame_time_t maxCharge = CalculateChargeTime(GetChargeLevel());
  if (activeForm) {
    maxCharge = activeForm->CalculateChargeTime(GetChargeLevel());
  }

  chargeEffect->SetMaxChargeTime(maxCharge);
  chargeEffect->SetCharging(state);
}

void Player::SetAttackLevel(unsigned lvl)
{
  stats.attack = std::min(PlayerStats::MAX_ATTACK_LEVEL, lvl);
}

const unsigned Player::GetAttackLevel()
{
  return stats.attack;
}

void Player::SetChargeLevel(unsigned lvl)
{
  stats.charge = std::min(PlayerStats::MAX_CHARGE_LEVEL, lvl);
}

const unsigned Player::GetChargeLevel()
{
  return stats.charge;
}

void Player::ModMaxHealth(int mod)
{
  stats.moddedHP += mod;
  SetMaxHealth(this->GetMaxHealth() + mod);
  SetHealth(this->GetMaxHealth());
}

const int Player::GetMaxHealthMod()
{
  return stats.moddedHP;
}

void Player::SetEmotion(Emotion emotion)
{
  this->emotion = emotion;

  if (this->emotion == Emotion::angry) {
    AddDefenseRule(superArmor);
  }
  else {
    RemoveDefenseRule(superArmor);
  }
}

Emotion Player::GetEmotion() const
{
  return emotion;
}

void Player::SetAnimation(string _state, std::function<void()> onFinish) {
  state = _state;

  if (state == PLAYER_IDLE) {
    animationComponent->SetAnimation(_state, Animator::Mode::Loop);
  }
  else {
    animationComponent->SetAnimation(_state, 0, onFinish);
  }

  animationComponent->Refresh();
}

const std::string Player::GetMoveAnimHash()
{
  return moveAnimHash;
}

const std::string Player::GetRecoilAnimHash()
{
  return recoilAnimHash;
}

void Player::SlideWhenMoving(bool enable, const frame_time_t& frames)
{
  slideFrames = frames;
  playerControllerSlide = enable;
}

std::shared_ptr<CardAction> Player::OnExecuteSpecialAction()
{
  if (specialOverride) {
    return specialOverride();
  }

  return nullptr;
}

frame_time_t Player::CalculateChargeTime(const unsigned chargeLevel)
{
  /**
  * These values include the 10i+ initial frames
  * 1 - 100i
  * 2 - 90i
  * 3 - 80i
  * 4 - 70i
  * 5 - 60i
  */
  switch (chargeLevel) {
  case 1:
    return frames(90);
  case 2:
    return frames(80);
  case 3:
    return frames(70);
  case 4:
    return frames(60);
  }

  return frames(50);
}

std::shared_ptr<CardAction> Player::ExecuteBuster()
{
   return OnExecuteBusterAction();
}

std::shared_ptr<CardAction> Player::ExecuteChargedBuster()
{
   return OnExecuteChargedBusterAction();
}

std::shared_ptr<CardAction> Player::ExecuteSpecial()
{
  return OnExecuteSpecialAction();
}

void Player::ActivateFormAt(int index)
{
  index = index - 1; // base 1 to base 0

  if (activeForm) {
    activeForm->OnDeactivate(shared_from_base<Player>());
    delete activeForm;
    activeForm = nullptr;
    RevertStats();
  }

  if (index >= 0 || index < forms.size()) {
    PlayerFormMeta* meta = forms[index];
    activeForm = meta->BuildForm();

    if (activeForm) {
      SaveStats();
      activeForm->OnActivate(shared_from_base<Player>());
      CreateMoveAnimHash();
      CreateRecoilAnimHash();
      animationComponent->Refresh();
    }
  }

  // Find nodes that do not have tags, those are newly added
  for (std::shared_ptr<SceneNode>& node : GetChildNodes()) {
    // if untagged and not the charge effect...
    if (!node->HasTag(Player::BASE_NODE_TAG) && node != chargeEffect) {
      // Tag it as a form node
      node->AddTags({ Player::FORM_NODE_TAG });
      node->EnableParentShader(true);
    }
  }
}

void Player::DeactivateForm()
{
  if (activeForm) {
    activeForm->OnDeactivate(shared_from_base<Player>());
    RevertStats();
    CreateMoveAnimHash();
  }
}

const bool Player::IsInForm() const
{
  return (this->activeForm != nullptr);
}

const std::vector<PlayerFormMeta*> Player::GetForms()
{
  auto res = std::vector<PlayerFormMeta*>();

  for (int i = 0; i < forms.size(); i++) {
    res.push_back(forms[i]);
  }

  return res;
}

ChargeEffectSceneNode& Player::GetChargeComponent()
{
  return *chargeEffect;
}

void Player::OverrideSpecialAbility(const std::function<std::shared_ptr<CardAction> ()>& func)
{
  specialOverride = func;
}

void Player::SaveStats()
{
  savedStats.element = GetElement();
  savedStats.charge = GetChargeLevel();
  savedStats.attack = GetAttackLevel();
}

void Player::RevertStats()
{
  SetChargeLevel(savedStats.charge);
  SetAttackLevel(savedStats.attack);
  SetElement(savedStats.element);
}

void Player::CreateMoveAnimHash()
{
  const auto i4_frames = frames(4);
  const auto i4_seconds = seconds_cast<float>(i4_frames);
  const auto i5_seconds = seconds_cast<float>(frames(5));
  const auto i1_seconds = seconds_cast<float>(frames(1));

  this->moveStartupDelay = frames(6);
  this->moveEndlagDelay = frames(7);
  auto frame_data = std::initializer_list<OverrideFrame>{
    { 1, i5_seconds },
    { 2, i1_seconds },
    { 3, i1_seconds },
    { 4, i1_seconds },
    { 3, i1_seconds },
    { 2, i1_seconds },
    { 1, i4_seconds }
  };

  // creates and stores the new state in variable `moveAnimHash`
  animationComponent->OverrideAnimationFrames("PLAYER_MOVE", frame_data, moveAnimHash);
}

void Player::CreateRecoilAnimHash()
{
  const auto i15_seconds = seconds_cast<float>(frames(15));
  const auto i7_seconds = seconds_cast<float>(frames(7));

  auto frame_data = std::initializer_list<OverrideFrame>{
    { 1, i15_seconds },
    { 2, i7_seconds }
  };

  // creates and stores the new state in variable `moveAnimHash`
  animationComponent->OverrideAnimationFrames("PLAYER_HIT", frame_data, recoilAnimHash);
}

void Player::TagBaseNodes()
{
  for (auto& node : GetChildNodes()) {
    // skip charge node, it is a special effect and not a part of our character
    if (node == chargeEffect) continue;

    node->AddTags({ Player::BASE_NODE_TAG });
  }
}

void Player::FinishConstructor()
{
  CreateMoveAnimHash();
  CreateRecoilAnimHash();
  TagBaseNodes();

  animationComponent->SetAnimation("PLAYER_IDLE");
  animationComponent->Refresh();
  animationComponent->SetPlaybackMode(Animator::Mode::Loop);

}

bool Player::RegisterForm(PlayerFormMeta* meta)
{
  if (forms.size() >= Player::MAX_FORM_SIZE || !meta) return false;
  forms.push_back(meta);
  return true;
}

PlayerFormMeta* Player::AddForm(PlayerFormMeta* meta)
{
  if (!RegisterForm(meta)) {
    delete meta;
    meta = nullptr;
  }

  return meta;
}
