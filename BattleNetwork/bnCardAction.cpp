#include "bnCardAction.h"
#include "battlescene/bnBattleSceneBase.h"

CardAction::CardAction(Character& actor, const std::string& animation) : 
  actor(actor),
  animation(animation), 
  anim(actor.GetFirstComponent<AnimationComponent>()),
  uuid(), 
  prevState(), 
  attachments(),
  ResourceHandle()
{
  if (anim) {
    prepareActionDelegate = [this] {
      for (auto& [nodeName, node] : attachments) {
        this->actor.AddNode(&node.spriteProxy.get());
        node.AttachAllPendingNodes();
      }

      // use the current animation's arrangement, do not overload
      prevState = anim->GetAnimationString();

      Logger::Logf("prevState was %s", prevState.c_str());

      anim->CancelCallbacks();

      if (!anim->GetAnimationObject().HasAnimation(this->animation)) {
        this->animation = prevState;
        Logger::Logf("Character %s did not have animation %s, reverting to last anim", this->actor.GetName().c_str(), this->animation.c_str());
      }

      anim->SetAnimation(this->animation, [this]() {
        RecallPreviousState();
      });

      anim->SetInterruptCallback([this]() {
        RecallPreviousState();
      });

      this->animationIsOver = false;
      anim->OnUpdate(0);
    };
  }
}

CardAction::~CardAction()
{
  stepList.clear();
  sequence.clear();

  if (started) {
    FreeAttachedNodes();
  }
}

void CardAction::AddStep(Step step)
{
  // Swooshlib needs pointers so we put these steps in a list and use that address
  auto iter = stepList.insert(stepList.begin(), new Step(step));
  sequence.add(*iter);
}

void CardAction::AddAnimAction(int frame, const FrameCallback& action) {
  Logger::Logf("AddAnimAction() called");
  anim->AddCallback(frame, action);
}

sf::Vector2f CardAction::CalculatePointOffset(const std::string& point) {
  assert(this->anim && "Character must have an animation component");

  return  this->anim->GetPoint(point) - this->anim->GetPoint("origin");
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

Character& CardAction::GetActor()
{
  return actor;
}

void CardAction::OverrideAnimationFrames(std::list<OverrideFrame> frameData)
{
  if (anim) {
    prepareActionDelegate = [this, frameData]() {
      for (auto& [nodeName, node] : attachments) {
        this->GetActor().AddNode(&node.spriteProxy.get());
        node.AttachAllPendingNodes();
      }

      anim->CancelCallbacks();

      prevState = anim->GetAnimationString();;
      Logger::Logf("(override animation) prevState was %s", prevState.c_str());

      if (!anim->GetAnimationObject().HasAnimation(this->animation)) {
        this->animation = prevState;
        Logger::Logf("(override animation) Character %s did not have animation %s, reverting to last anim", this->actor.GetName().c_str(), this->animation.c_str());
      }

      anim->OverrideAnimationFrames(this->animation, frameData, uuid);
      anim->SetAnimation(uuid, [this]() {
        RecallPreviousState();
      });

      anim->SetInterruptCallback([this]() {
        RecallPreviousState();
      });

      this->animationIsOver = false;
      anim->OnUpdate(0);
    };
  }
}

void CardAction::Execute(Character* user)
{
  // prepare the animation behavior
  prepareActionDelegate();
  started = true;
  // run
  OnExecute(user);
  // Position any new nodes to actor
  Update(0);
}

void CardAction::EndAction()
{
  RecallPreviousState();
  OnActionEnd();
}

CardAction::Attachment& CardAction::AddAttachment(Animation& parent, const std::string& point, SpriteProxyNode& node) {
  auto iter = attachments.insert(std::make_pair(point, Attachment{ std::ref(parent), point, std::ref(node) }));

  if (started) {
    auto& actor = this->GetActor();
    actor.AddNode(&node);
    
    // inform any new attachments they can and should attach immediately
    iter->second.started = true;
  }

  return iter->second;
}

CardAction::Attachment& CardAction::AddAttachment(Character& character, const std::string& point, SpriteProxyNode& node) {
  auto animComp = character.GetFirstComponent<AnimationComponent>();
  assert(animComp && "character must have an animation component");

  if (started) {
    this->GetActor().AddNode(&node);
  }

  return AddAttachment(animComp->GetAnimationObject(), point, node);
}

void CardAction::Update(double _elapsed)
{
  for (auto& [nodeName, node] : attachments) {
    // update the node's animation
    node.Update(_elapsed);

    // update the node's position
    auto baseOffset = node.GetParentAnim().GetPoint(nodeName);
    const auto& origin = actor.getSprite().getOrigin();
    baseOffset = baseOffset - origin;

    node.SetOffset(baseOffset);

    /*
      NOTE: (7/25/2021)
      This is hacky. Should have a standard that everything faces right per default
      and engine flips everything based on Team and Facing direction.
      May come to haunt me later so here's a reminder to take this out.
    */
    if (this->actor.GetFacing() == Direction::left) {
      auto scale = sf::Vector2f{ -1.0, 1.0 };
      node.SetScale(scale);
    }
  }

  if (!started) return;

  if (lockoutProps.type == CardAction::LockoutType::sequence) {
    sequence.update(sf::seconds(_elapsed));
    if (sequence.isEmpty()) {
      animationIsOver = true; // animation for sequence is complete
      RecallPreviousState();
      EndAction();
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
  this->lockoutProps = props;
}

void CardAction::SetLockoutGroup(const CardAction::LockoutGroup& group)
{
  lockoutProps.group = group;
}

void CardAction::FreeAttachedNodes() {
  for (auto& [nodeName, node] : attachments) {
    this->GetActor().RemoveNode(&node.spriteProxy.get());
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
  return this->animation;
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

//////////////////////////////////////////////////
//                Attachment Impl               //
//////////////////////////////////////////////////

CardAction::Attachment::Attachment(Animation& parentAnim, const std::string& point, SpriteProxyNode& parentNode) :
  spriteProxy(parentNode), parentAnim(parentAnim), point(point)
{
}

CardAction::Attachment::~Attachment()
{
}

CardAction::Attachment& CardAction::Attachment::UseAnimation(Animation& anim)
{
  myAnim = &anim;
  anim.Refresh(spriteProxy.get().getSprite());

  // update the node's position
  auto baseOffset = GetParentAnim().GetPoint(point);
  const auto& origin = spriteProxy.get().getSprite().getOrigin();
  baseOffset = baseOffset - origin;
  SetOffset(baseOffset);

  return *this;
}

void CardAction::Attachment::Update(double elapsed)
{
  if (this->myAnim) {
    this->myAnim->Update(elapsed, spriteProxy.get().getSprite());
  }

  for (auto& [nodeName, node] : attachments) {
    // update the node's animation
    node.Update(elapsed);

    // update the node's position
    auto baseOffset = node.GetParentAnim().GetPoint(nodeName);
    const auto& origin = spriteProxy.get().getSprite().getOrigin();
    baseOffset = baseOffset - origin;

    node.SetOffset(baseOffset);
  }
}

void CardAction::Attachment::SetOffset(const sf::Vector2f& pos)
{
  this->spriteProxy.get().setPosition(pos);
}

void CardAction::Attachment::SetScale(const sf::Vector2f& scale)
{
  this->spriteProxy.get().setScale(scale);
}

void CardAction::Attachment::AttachAllPendingNodes()
{
  for (auto& [nodeName, node] : attachments) {
    this->spriteProxy.get().AddNode(&node.spriteProxy.get());
  }

  started = true;
}

Animation& CardAction::Attachment::GetParentAnim()
{
  return this->parentAnim;
}

CardAction::Attachment& CardAction::Attachment::AddAttachment(Animation& parent, const std::string& point, SpriteProxyNode& node) {
  auto iter = attachments.insert(std::make_pair(point, Attachment(std::ref(parent), point, std::ref(node) )));

  if (started) {
    this->spriteProxy.get().AddNode(&node);

    // inform any new attachments they can and should attach immediately
    iter->second.started = true;
  }

  return iter->second;
}