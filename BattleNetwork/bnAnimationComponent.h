#pragma once
#include <string>
#include <assert.h>
#include <functional>

#include "bnComponent.h"
#include "bnAnimation.h"

using std::string;
using std::to_string;

class Entity;
class Character;
class SpriteProxyNode;

/**
 * @class AnimationComponent
 * @author mav
 * @date 14/05/19
 *
 * A component to add to entities to provide animations.
 *
 */
class AnimationComponent : public Component {
public:
  /**
  * @brief struct that references the other animation, point to sync with, and sprite to apply to
  */
  struct SyncItem {
    Animation* anim{ nullptr };
    std::shared_ptr<SpriteProxyNode> node{ nullptr };
    std::string point{ "origin" };
  };

  /**
   @param _entity Owner of this component
    */
  AnimationComponent(std::weak_ptr<Entity> _entity);
  ~AnimationComponent();

  /**
   * @brief Delegates work to animation object
   * @param _elapsed in seconds
   */
  void OnUpdate(double _elapsed) override;

  /**
   * @brief Does not inject into scene. Used by the owner.
   * @param BattleSceneBase& unused
   */
  void Inject(BattleSceneBase&) override { ; }
  
  /**
   * @brief Reconstructs the animation object
   * @param _path to animation file
   */
  void SetPath(string _path);

  /**
   * @brief Loads the animation object
   */
  void Load();

  /**
   * @brief Reload the animation object. Same as Load()
   */
  void Reload();

  /**
  * @brief Do not load from path, copy from an existing animation
  */
  void CopyFrom(const AnimationComponent& rhs);
  
  /**
   * @brief Get animation object's current animation name
   * @return name of animation or empty string if no state
   */
  const std::string GetAnimationString() const;
  
  /**
   * @brief Get animation object's file path used to setup the animation
   * @returnpath of animation file or empty string if no file was loaded
   */
  const std::string& GetFilePath() const;

  /**
   * @brief Set the animation playback speed
   * @param playbackSpeed factor of 100 e.g. 2 is 200%, 0.5 is 50%
   */
  void SetPlaybackSpeed(const double playbackSpeed);

  /**
   * @brief Fetch the animation playback speed 
   * @return double playback speed 
   */
  const double GetPlaybackSpeed();
  
  /**
   * @brief Set the playback mode
   * @param playbackMode byte mode
   */
  void SetPlaybackMode(char playbackMode);
  
  /**
   * @brief Set the animation and provide an on finish notifier
   * @param state
   */
  void SetAnimation(string state, FrameFinishCallback onFinish = nullptr);
  
  /**
   * @brief Set the animation, set the playback mode, and provide an on finish notifier
   * @param state
   * @param playbackMode
   */
  void SetAnimation(string state, char playbackMode, FrameFinishCallback onFinish = Animator::NoCallback);
  
  /**
   * @brief Add a frame callback
   * @param frame Base 1 index
   * @param onFrame Callback when frame is entered
   * @param doOnce Toggle if the callback should be fired every time the animation loops or just once
   */
  void AddCallback(int frame, FrameCallback onFrame, bool doOnce = false);
  
  /**
   * @brief adds a callback to make the frame toggle counterable = true for Characters
   * @param frameStart Base 1 index
   * @param frameEnd Base 1 index
   *
   * Includes all frames from range (start, end). Effectively frame end+1 will set counterable = false
   * @warning If an invalid range is provided the operation will be ignored
   */
  void SetCounterFrameRange(int frameStart, int frameEnd);

  /**
   * @brief Clears callbacks
   */
  void CancelCallbacks();

  void OnFinish(const FrameFinishCallback& onFinish);

  /**
   * @brief Get the (x,y) coordinate of a point from the current frame
   * @param pointName the name of the point in the animation file 
   * @return (x,y) vector of point or (0,0) if no point found
   */
  sf::Vector2f GetPoint(const std::string& pointName);

  const bool HasPoint(const std::string& pointName);

  Animation& GetAnimationObject();
  
  void OverrideAnimationFrames(const std::string& animation, std::list<OverrideFrame> data, std::string& uuid);

  void SyncAnimation(Animation& other);
  void SyncAnimation(std::shared_ptr<AnimationComponent> other);

  void AddToSyncList(const AnimationComponent::SyncItem& item);
  void RemoveFromSyncList(const AnimationComponent::SyncItem& item);
  std::vector<SyncItem> GetSyncItems();

  void SetInterruptCallback(const FrameFinishCallback& onInterrupt);
  /**
   * @brief Force the animation to jump to this frame index 
   * @param index index of the frame
   * If the index is out of range, sets frame to 0
   */
  void SetFrame(const int index);

  void Refresh();
private:
  string path; /*!< Path to animation */
  Animation animation; /*!< Animation object */
  bool couldUpdateLastFrame{ true };
  std::vector<SyncItem> syncList;

  void RefreshSyncItem(SyncItem& item);
  void UpdateAnimationObjects(sf::Sprite& sprite, double elapsed);
};
