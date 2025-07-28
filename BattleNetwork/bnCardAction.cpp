#include "bnCharacter.h"
#include "bnCardAction.h"
#include "battlescene/bnBattleSceneBase.h"

// wrapper for step, as swoosh expects raw pointers and takes ownership (deletes)
// causing double deletes if we want to share ownership
class StepActionItem : public swoosh::BlockingActionItem {
public:
  StepActionItem(std::shared_ptr<CardAction::Step> step) : step(step) {}

private:
  std::shared_ptr<CardAction::Step> step;

  // inherited functions simply invoke the functors
  void update(sf::Time elapsed) override {
    if (step->updateFunc) step->updateFunc(step, elapsed.asSeconds());
    if (step->IsComplete()) markDone();
  }
  void draw(sf::RenderTexture& surface) override {
    if (step->drawFunc) step->drawFunc(step, surface);
  }
};


CardAction::CardAction(std::weak_ptr<Character> user, const std::string& animation) : 
  actor(user),
  animation(animation), 
  anim(user.lock()->GetFirstComponent<AnimationComponent>()),
  uuid(), 
  prevState(), 
  attachments(),
  ResourceHandle()
{
  if (anim) {
    prepareActionDelegate = [this] {
      std::shared_ptr<Character> actor = this->GetActor();

      if (!actor) {
        return;
      }

      for (auto& [nodeName, node] : attachments) {
        actor->AddNode(node.spriteProxy);
        node.AttachAllPendingNodes();
      }

      // use the current animation's arrangement, do not overload
      prevState = anim->GetAnimationString();

      Logger::Logf(LogLevel::debug, "prevState was %s", prevState.c_str());

      anim->CancelCallbacks();

      if (!anim->GetAnimationObject().HasAnimation(this->animation)) {
        this->animation = prevState;
        Logger::Logf(LogLevel::debug, "Character %s did not have animation %s, reverting to last anim", actor->GetName().c_str(), this->animation.c_str());
      }

      anim->SetAnimation(this->animation, [this]() {
        RecallPreviousState();
      });

      anim->SetInterruptCallback([this]() {
        RecallPreviousState();
      });

      // add animation callbacks
      for (auto& [frame, action] : animActions) {
        anim->AddCallback(frame, action);
      }

      this->animationIsOver = false;
      anim->OnUpdate(0);
    };
  }
}

CardAction::~CardAction()
{
  sequence.clear();

  if (started) {
    FreeAttachedNodes();
  }
}

void CardAction::AddStep(std::shared_ptr<Step> step)
{
  if (step->Added()) {
    throw std::runtime_error("Step has already been added to a CardAction");
  }
  step->added = true;

  steps.push_back(step);
  // Swooshlib takes ownership of raw pointers so we wrap our actions
  sequence.add(new StepActionItem(step));
}

void CardAction::AddAnimAction(int frame, const FrameCallback& action) {
  Logger::Logf(LogLevel::debug, "AddAnimAction() called");

  animActions.emplace_back(frame, action);

  if (started) {
    anim->AddCallback(frame, action);
  }
}

sf::Vector2f CardAction::CalculatePointOffset(const std::string& point) {
  if (!anim) {
    if (std::shared_ptr<Character> actor = GetActor()) {
      Logger::Logf(LogLevel::warning, "Character %s must have an animation component", actor->GetName().c_str());
    }
  }

  return anim->GetPoint(point) - anim->GetPoint("origin");
}

void CardAction::RecallPreviousState()
{
  if (!recalledAnimation) {
    recalledAnimation = true;

    if (this->lockoutProps.type != CardAction::LockoutType::sequence) {
      animationIsOver = true;
    }

    FreeAttachedNodes();

    if (anim) {
      anim->SetAnimation(prevState);
      anim->SetPlaybackMode(Animator::Mode::Loop);
      OnAnimationEnd();
    }
  }
}

std::shared_ptr<Character> CardAction::GetActor()
{
  return actor.lock();
}

const std::shared_ptr<Character> CardAction::GetActor() const
{
  return actor.lock();
}

const Battle::Card::Properties& CardAction::GetMetaData() const
{
  return meta;
}

void CardAction::OverrideAnimationFrames(std::list<OverrideFrame> frameData)
{
  if (anim) {
    prepareActionDelegate = [this, frameData]() {
      std::shared_ptr<Character> actor = this->GetActor();

      if (!actor) {
        return;
      }

      for (auto& [nodeName, node] : attachments) {
        actor->AddNode(node.spriteProxy);
        node.AttachAllPendingNodes();
      }

      anim->CancelCallbacks();

      prevState = anim->GetAnimationString();;
      Logger::Logf(LogLevel::info, "(override animation) prevState was %s", prevState.c_str());

      if (!anim->GetAnimationObject().HasAnimation(this->animation)) {
        this->animation = prevState;
        Logger::Logf(LogLevel::info, "(override animation) Character %s did not have animation %s, reverting to last anim", actor->GetName().c_str(), this->animation.c_str());
      }

      anim->OverrideAnimationFrames(this->animation, frameData, uuid);
      anim->SetAnimation(uuid, [this]() {
        RecallPreviousState();
      });

      anim->SetInterruptCallback([this]() {
        RecallPreviousState();
      });

      // add animation callbacks
      for (auto& [frame, action] : animActions) {
        anim->AddCallback(frame, action);
      }

      this->animationIsOver = false;
      anim->OnUpdate(0);
    };
  }
}

void CardAction::SetMetaData(const Battle::Card::Properties& props)
{
  meta = props;
}

void CardAction::Execute(std::shared_ptr<Character> user)
{
  // prepare the animation behavior
  prepareActionDelegate();
  started = true;
  startTile = user->GetTile();
  startTile->ReserveEntityByID(user->GetID());

  // set attack context
  Hit::Context context;
  context.aggressor = user->GetID();

  if (preventCounters) {
    context.flags |= Hit::no_counter;
  }

  user->SetHitboxContext(context);
  userWeak = user;

  // run
  OnExecute(user);
  // Position any new nodes to actor
  Update(0);
}

void CardAction::EndAction()
{
  if (!started) return;

  RecallPreviousState();
  OnActionEnd();
  
  if (std::shared_ptr<Character> actorPtr = actor.lock()) {
    actorPtr->GetTile()->RemoveEntityByID(actorPtr->GetID());
    startTile->AddEntity(actorPtr);
  }

  if (std::shared_ptr<Character> user = userWeak.lock()) {
    // reset context
    Hit::Context context;
    context.aggressor = user->GetID();
    context.flags = Hit::none;
    user->SetHitboxContext(context);
  }
}

void CardAction::UseStuntDouble(std::shared_ptr<Character> stuntDouble)
{
  actor = stuntDouble;
  
  if (std::shared_ptr<AnimationComponent> stuntDoubleAnim = stuntDouble->GetFirstComponent<AnimationComponent>()) {
    for (auto& [nodeName, node] : attachments) {
      node.parentAnim = stuntDoubleAnim->GetAnimationObject();
    }

    anim = stuntDoubleAnim;
  }
}

CardAction::Attachment& CardAction::AddAttachment(const std::string& point) {
  std::shared_ptr<Character> actor = GetActor();

  if (!actor) {
    throw std::runtime_error("Owner was deleted");
  }

  std::shared_ptr<AnimationComponent> animComp = actor->GetFirstComponent<AnimationComponent>();
  assert(animComp && "owner must have an animation component");
  Animation& anim = animComp->GetAnimationObject();

  auto iter = attachments.insert(std::make_pair(point, Attachment{ anim, point }));

  if (started) {
    actor->AddNode(iter->second.spriteProxy);

    // inform any new attachments they can and should attach immediately
    iter->second.started = true;
  }

  return iter->second;
}

void CardAction::Update(double _elapsed)
{
  std::shared_ptr<Character> actor = GetActor();

  if (!actor) {
    return;
  }

  for (auto& [nodeName, node] : attachments) {
    // update the node's animation
    node.Update(_elapsed);

    // update the node's position
    sf::Vector2f baseOffset = node.GetParentAnim().GetPoint(nodeName);
    const sf::Vector2f& origin = actor->getSprite().getOrigin();
    baseOffset = baseOffset - origin;

    node.SetOffset(baseOffset);
  }

  if (!started) return;

  if (lockoutProps.type == CardAction::LockoutType::sequence) {
    sequence.update(sf::seconds(_elapsed));
    if (sequence.isEmpty()) {
      animationIsOver = true; // animation for sequence is complete
      RecallPreviousState();
    }
  }
  else if(animationIsOver) /* lockoutProps.type == CardAction::LockoutType::async */ {
    RecallPreviousState();

    lockoutProps.cooldown -= _elapsed;
    lockoutProps.cooldown = std::max(lockoutProps.cooldown, 0.0);
  }
}

void CardAction::draw(sf::RenderTarget& target, sf::RenderStates states) const {
  /* silence is golden */
};

void CardAction::PreventCounters()
{
  preventCounters = true;
}

void CardAction::SetLockout(const CardAction::LockoutProperties& props)
{
  lockoutProps = props;
}

void CardAction::SetLockoutGroup(const CardAction::LockoutGroup& group)
{
  lockoutProps.group = group;
}

void CardAction::FreeAttachedNodes() {
  if (std::shared_ptr<Character> actor = GetActor()) {
    for (auto& [nodeName, node] : attachments) {
      actor->RemoveNode(node.spriteProxy.get());
    }
  }

  attachments.clear();
}

const CardAction::LockoutGroup CardAction::GetLockoutGroup() const
{
  return lockoutProps.group;
}

const CardAction::LockoutType CardAction::GetLockoutType() const
{
  return lockoutProps.type;
}

const std::string& CardAction::GetAnimState() const
{
  return animation;
}

const bool CardAction::IsAnimationOver() const
{
  return animationIsOver;
}

const bool CardAction::IsLockoutOver() const {
  // animation and sequence lockout type checks
  if (lockoutProps.type != CardAction::LockoutType::async) {
    return IsAnimationOver();
  }

  return IsAnimationOver() && lockoutProps.cooldown <= 0;
}

const bool CardAction::CanExecute() const
{
  return started == false;
}

std::optional<bool> CardAction::CanMoveTo(Battle::Tile* next) {
  return {};
}

//////////////////////////////////////////////////
//                Attachment Impl               //
//////////////////////////////////////////////////

CardAction::Attachment::Attachment(Animation& parentAnim, const std::string& point) :
  spriteProxy(std::make_shared<SpriteProxyNode>()), parentAnim(parentAnim), point(point)
{
}

CardAction::Attachment::~Attachment()
{
}

void CardAction::Attachment::Update(double elapsed)
{
  animation.Update(elapsed, spriteProxy->getSprite());

  for (auto& [nodeName, node] : attachments) {
    // update the node's animation
    node.Update(elapsed);

    // update the node's position
    sf::Vector2f baseOffset = node.GetParentAnim().GetPoint(nodeName);
    const sf::Vector2f& origin = spriteProxy->getSprite().getOrigin();
    baseOffset = baseOffset - origin;

    node.SetOffset(baseOffset);
  }
}

void CardAction::Attachment::SetOffset(const sf::Vector2f& pos)
{
  spriteProxy->setPosition(pos);
}

void CardAction::Attachment::SetScale(const sf::Vector2f& scale)
{
  spriteProxy->setScale(scale);
}

void CardAction::Attachment::AttachAllPendingNodes()
{
  for (auto& [nodeName, node] : attachments) {
    spriteProxy->AddNode(node.spriteProxy);
  }

  started = true;
}

Animation& CardAction::Attachment::GetParentAnim()
{
  return parentAnim;
}

CardAction::Attachment& CardAction::Attachment::AddAttachment(const std::string& point) {
  auto iter = attachments.insert(std::make_pair(point, Attachment(animation, point)));

  if (started) {
    spriteProxy->AddNode(iter->second.spriteProxy);

    // inform any new attachments they can and should attach immediately
    iter->second.started = true;
  }

  return iter->second;
}

std::shared_ptr<SpriteProxyNode> CardAction::Attachment::GetSpriteNode() {
  return spriteProxy;
}


Animation& CardAction::Attachment::GetAnimationObject() {
  return animation;
}
