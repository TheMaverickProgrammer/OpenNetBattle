#include <SFML/Graphics.hpp>
using sf::Sprite;
using sf::IntRect;

#include "bnAnimationComponent.h"
#include "bnSpriteProxyNode.h"
#include "bnFileUtil.h"
#include "bnLogger.h"
#include "bnEntity.h"
#include "bnCharacter.h"

AnimationComponent::AnimationComponent(std::weak_ptr<Entity> _entity) : Component(_entity) {}

AnimationComponent::~AnimationComponent() {
}

void AnimationComponent::OnUpdate(double _elapsed)
{
  std::shared_ptr<Entity> owner = GetOwner();

  // stunned entities do not update
  if (!owner) return;

  // `couldUpdateLastFrame` is used to allow animationComponent to update our frame-sensitive animations
  // on the conditions that the entity would-be stunned this frame, the animation component will need present that correct frame outcome
  // before halting due to status effects
  bool canUpdateThisFrame = (!(owner->IsStunned() || owner->IsIceFrozen())) && couldUpdateLastFrame;
  if (!canUpdateThisFrame) {
    return;
  }

  couldUpdateLastFrame = canUpdateThisFrame;
  UpdateAnimationObjects(owner->getSprite(), _elapsed);
}

void AnimationComponent::SetPath(string _path)
{
  path = _path;
}

void AnimationComponent::Load() {
  Reload();
}

void AnimationComponent::Reload() {
  animation = Animation(path);
  animation.Reload();
}

void AnimationComponent::CopyFrom(const AnimationComponent& rhs)
{
  animation = rhs.animation;
  path = rhs.path;
}

const std::string AnimationComponent::GetAnimationString() const
{
  return animation.GetAnimationString();
}

const std::string& AnimationComponent::GetFilePath() const
{
  return path;
}

void AnimationComponent::SetPlaybackSpeed(const double playbackSpeed)
{
  animation.SetPlaybackSpeed(playbackSpeed);
}

const double AnimationComponent::GetPlaybackSpeed()
{
  return animation.GetPlaybackSpeed();
}

void AnimationComponent::SetAnimation(string state, FrameFinishCallback onFinish)
{
  animation.SetAnimation(state);

  for (auto& s : syncList) {
    s.anim->SetAnimation(state);
    s.anim->Refresh(s.node->getSprite());
    RefreshSyncItem(s);
  }

  animation << onFinish;

  if (auto owner = GetOwner()) {
    animation.Refresh(owner->getSprite());
  }
}

void AnimationComponent::SetAnimation(string state, char playbackMode, FrameFinishCallback onFinish)
{
  animation.SetAnimation(state);

  for (auto& s : syncList) {
    s.anim->SetAnimation(state);
    s.anim->Refresh(s.node->getSprite());
    RefreshSyncItem(s);
  }

  animation << playbackMode << onFinish;

  if (std::shared_ptr<Entity> owner = GetOwner()) {
    animation.Refresh(owner->getSprite());
  }
}

void AnimationComponent::SetPlaybackMode(char playbackMode)
{
  animation << playbackMode;

  for (auto& s : syncList) {
    (*s.anim) << playbackMode;
  }
}

void AnimationComponent::AddCallback(int frame, FrameCallback onFrame, bool doOnce) {
  animation << Animator::On(frame, onFrame, doOnce);
}

void AnimationComponent::SetCounterFrameRange(int frameStart, int frameEnd)
{
  if (frameStart == frameEnd) frameEnd = frameEnd + 1;
  if (frameStart > frameEnd || frameStart <= 0 || frameEnd <= 0) return;

  std::shared_ptr<Character> c = GetOwnerAs<Character>();
  if (c == nullptr) return;

  auto enableCounterable  = [c]() { c->ToggleCounter(); };
  auto disableCounterable = [c]() { c->ToggleCounter(false); };

  animation << Animator::On(frameStart, enableCounterable, true);
  animation << Animator::On(frameEnd,  disableCounterable, true);

}

void AnimationComponent::CancelCallbacks()
{
  // We just want to cancel callbacks
  // Preserve some other state info
  auto mode = animation.GetMode();
  animation.RemoveCallbacks();
  animation << mode;

  for (auto& s : syncList) {
    s.anim->RemoveCallbacks();
    (*s.anim) << mode;
  }
}

void AnimationComponent::OnFinish(const FrameFinishCallback& onFinish)
{
  CancelCallbacks();
  animation << onFinish;
}

sf::Vector2f AnimationComponent::GetPoint(const std::string & pointName)
{
  return animation.GetPoint(pointName);
}

const bool AnimationComponent::HasPoint(const std::string& pointName)
{
  return animation.HasPoint(pointName);
}

Animation & AnimationComponent::GetAnimationObject()
{
  return animation;
}

void AnimationComponent::OverrideAnimationFrames(const std::string& animation, std::list<OverrideFrame> data, std::string & uuid)
{
  AnimationComponent::animation.OverrideAnimationFrames(animation, data, uuid);

  for (auto& o : syncList) {
    o.anim->OverrideAnimationFrames(animation, data, uuid);
  }
}

void AnimationComponent::SyncAnimation(Animation & other)
{
  animation.SyncAnimation(other);
}

void AnimationComponent::SyncAnimation(std::shared_ptr<AnimationComponent> other)
{
  animation.SyncAnimation(other->animation);
  other->OnUpdate(0);
}

void AnimationComponent::AddToSyncList(const AnimationComponent::SyncItem& item)
{
  auto iter = std::find_if(syncList.begin(), syncList.end(), [item](auto in) {
    return std::tie(in.anim, in.node, in.point) == std::tie(item.anim, item.node, item.point); 
  });

  if (iter == syncList.end()) {
    syncList.push_back(item);
  }
}

void AnimationComponent::RemoveFromSyncList(const AnimationComponent::SyncItem& item)
{
  auto iter = std::find_if(syncList.begin(), syncList.end(), [item](auto in) {
    return std::tie(in.anim, in.node, in.point) == std::tie(item.anim, item.node, item.point);
  });

  if (iter != syncList.end()) {
    syncList.erase(iter);
  }
}

std::vector<AnimationComponent::SyncItem> AnimationComponent::GetSyncItems()
{
  return syncList;
}

void AnimationComponent::SetInterruptCallback(const FrameFinishCallback& onInterrupt)
{
  animation.SetInterruptCallback(onInterrupt);
}

void AnimationComponent::SetFrame(const int index)
{
  if (std::shared_ptr<Entity> owner = GetOwner())
  animation.SetFrame(index, owner->getSprite());

  for (auto& s : syncList) {
    s.anim->SetFrame(index, s.node->getSprite());
    RefreshSyncItem(s);
  }
}

void AnimationComponent::Refresh()
{
  std::shared_ptr<Entity> owner = GetOwner();
  if (!owner) return;

  UpdateAnimationObjects(owner->getSprite(), 0);
}

void AnimationComponent::RefreshSyncItem(AnimationComponent::SyncItem& item)
{
  auto character = GetOwnerAs<Character>();

  if (!character) {
    return;
  }

  // update node position in the animation
  auto baseOffset = GetPoint(item.point);
  const sf::Vector2f& origin = character->getSprite().getOrigin();
  baseOffset = baseOffset - origin;

  item.node->setPosition(baseOffset);
}

void AnimationComponent::UpdateAnimationObjects(sf::Sprite& sprite, double elapsed)
{
  animation.Update(elapsed, sprite);

  for (auto& s : syncList) {
    animation.SyncAnimation(*s.anim);
    s.anim->Refresh(s.node->getSprite());
    RefreshSyncItem(s);
  }
}
