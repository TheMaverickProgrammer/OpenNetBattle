#pragma once
#include <Swoosh/ActionList.h>
#include <SFML/Graphics.hpp>
#include <string>
#include <functional>
#include <optional>
#include <map>
#include "stx/memory.h"

#include "bnSpriteProxyNode.h"
#include "bnResourceHandle.h"
#include "bnAnimationComponent.h"
#include "bnCard.h"

class Character;

namespace Battle {
  class Tile;
}

using namespace swoosh;

class CardAction : public stx::enable_shared_from_base<CardAction>, public sf::Drawable, public ResourceHandle {
public:
  enum class LockoutType : unsigned {
    animation = 0,
    async,
    sequence
  };

  enum class LockoutGroup : unsigned {
    none = 0,
    weapon,
    card,
    ability
  };

  struct LockoutProperties {
    LockoutType type{};
    double cooldown{}; // in seconds
    LockoutGroup group{ LockoutGroup::card };
  };

  class Attachment {
    using Attachments = std::multimap<std::string, Attachment>;

    bool started{ false };
    std::string point;
    std::shared_ptr<SpriteProxyNode> spriteProxy;
    std::reference_wrapper<Animation> parentAnim;
    Attachments attachments;
    Animation animation;

  public:
    friend class CardAction; // Let CardAction inspect our member vars

    Attachment(Animation& parentAnim, const std::string& point);
    ~Attachment();

    void Update(double elapsed);
    void SetOffset(const sf::Vector2f& pos);
    void SetScale(const sf::Vector2f& scale);
    void AttachAllPendingNodes();
    Animation& GetParentAnim();
    Attachment& AddAttachment(const std::string& point);
    std::shared_ptr<SpriteProxyNode> GetSpriteNode();
    Animation& GetAnimationObject();
  };

  struct Step {
    using UpdateFunc_t = std::function<void(std::shared_ptr<Step>, double)>;
    using DrawFunc_t = std::function<void(std::shared_ptr<Step>, sf::RenderTexture&)>;
    
    constexpr static auto NoUpdateFunc = [](std::shared_ptr<Step>, double) -> void {};
    constexpr static auto NoDrawFunc = [](std::shared_ptr<Step>, sf::RenderTexture&) -> void {};

    UpdateFunc_t updateFunc;
    DrawFunc_t drawFunc;

    Step(const UpdateFunc_t& u = NoUpdateFunc, const DrawFunc_t& d = NoDrawFunc) :
      updateFunc(u), drawFunc(d) {}

    void CompleteStep() {
      complete = true;
    }

    bool IsComplete() {
      return complete;
    }

    bool Added() {
      return added;
    }

    private:
      bool complete{};
      bool added{};
      friend class CardAction;
  };

  using Attachments = std::multimap<std::string, Attachment>;

private:
  bool animationIsOver{ false };
  bool started{ false };
  bool recalledAnimation{ false };
  bool preventCounters{ false };
  LockoutProperties lockoutProps{};
  std::string animation;
  std::string uuid, prevState;
  std::function<void()> prepareActionDelegate;
  ActionList sequence;
  std::vector<std::shared_ptr<Step>> steps;
  std::weak_ptr<Character> actor;
  std::weak_ptr<Character> userWeak;
  Attachments attachments;
  std::shared_ptr<AnimationComponent> anim{ nullptr };
  Battle::Card meta;
  std::vector<std::pair<int, FrameCallback>> animActions;
  Battle::Tile* startTile{ nullptr };

  // Used internally
  void RecallPreviousState();
  void FreeAttachedNodes();

public:
  CardAction() = delete;
  CardAction(const CardAction& rhs) = delete;
  CardAction(std::weak_ptr<Character> actor, const std::string& animation);
  virtual ~CardAction();

  // Used by cards that use sequences (like most Time Freeze animations)
  void AddStep(std::shared_ptr<Step> step);

  // Used with basic cards that perform some action
  void AddAnimAction(int frame, const FrameCallback& action);

  // For additional visual overlays
  Attachment& AddAttachment(const std::string& point);

  // Calculate the offset for an attachment for a given point in the owner's animation set
  sf::Vector2f CalculatePointOffset(const std::string& point);

  void PreventCounters();
  void SetLockout(const LockoutProperties& props);
  void SetLockoutGroup(const LockoutGroup& group);
  void OverrideAnimationFrames(std::list<OverrideFrame> frameData);
  void SetMetaData(const Battle::Card& card);
  void Execute(std::shared_ptr<Character> user);
  void EndAction();
  void UseStuntDouble(std::shared_ptr<Character> stuntDouble); // can cause GetActor to return nullptr

  const LockoutGroup GetLockoutGroup() const;
  const LockoutType GetLockoutType() const;
  const std::string& GetAnimState() const;
  const bool IsAnimationOver() const;
  const bool IsLockoutOver() const;
  const Battle::Card& GetMetaData() const;
  const bool CanExecute() const;
  std::shared_ptr<Character> GetActor(); // may return nullptr
  const std::shared_ptr<Character> GetActor() const; // may return nullptr

  virtual void Update(double _elapsed);
  virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
  virtual std::optional<bool> CanMoveTo(Battle::Tile* next);
protected:
  virtual void OnActionEnd() = 0;
  virtual void OnAnimationEnd() = 0;
  virtual void OnExecute(std::shared_ptr<Character> user) = 0;
};
