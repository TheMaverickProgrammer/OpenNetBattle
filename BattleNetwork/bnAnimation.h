#pragma once
#include <string>
#include <assert.h>
#include <functional>

#include <iostream>

#include "bnAnimator.h"

using std::string;
using std::to_string;

#define ANIMATION_EXTENSION ".animation"

/**
 * @class Animation
 * @author mav
 * @date 14/05/19
 * @see bnAnimator.h
 * @brief Loads a FrameList from an animation file format and animates a sprite
 * 
 * Basically a wrapper around the animator that knows the possible animations.
 * 
 * File format example:
 * 
 * VERSION="1.0"
 * imagePath=C:\User\Bob\Desktop\player.png
 *
 * ```
 * animation state="PLAYER_SHOOTING"
 * frame duration="0.05" x="0" y="10" w="70" h="47" originx="21" originy="40" flipx="0" flipy="0"
 * point label="Buster" x="42" y="15"
 * frame duration="0.05" x="72" y="7" w="70" h="47" originx="23" originy="40" flipx="0" flipy="0"
 * point label="Buster" x="42" y="15"
 * frame duration="0.05" x="149" y="11" w="75" h="47" originx="21" originy="39" flipx="0" flipy="0"
 * point label="Buster" x="40" y="14"
 * frame duration="0.05" x="225" y="11" w="77" h="47" originx="20" originy="39" flipx="0" flipy="0"
 * point label="Buster" x="39" y="14"
 * frame duration="0.05" x="302" y="11" w="68" h="47" originx="18" originy="39" flipx="0" flipy="0"
 * point label="Buster" x="38" y="14"
 * 
 * animation state="PLAYER_MOVING"
 * ...
 * ```
 *
 * etc.
 */
class Animation {
public:
  /**
   * @brief No frame list is loaded*/
  Animation();
  
  /**
   * @brief Parses file at path and populates FrameList with data
   * @param path relative path from application to file
   */
  Animation(const char* path);
  
  /**
   * @brief Parses file at path and populates FrameList with data
   * @param path relative path from application to file
   */
  Animation(const string& path);

  Animation(const Animation& rhs);

  Animation& operator=(const Animation& rhs);

  ~Animation();

  void CopyFrom(const Animation& rhs);

  /**
   * @brief Reads file at path set by constructor, parses lines, and populates FrameList with data

     Effectively same as calling Load()
   */
  void Reload();
 
  /**
 * @brief Reads file at path set by constructor, parses lines, and populates FrameList with data

    Effectively same as calling Reload();
 */
  void Load(const std::string& newPath = "");
 
  /**
 * @brief Parses lines, and populates FrameList with data

    Effectively same as calling Reload();
 */
  void LoadWithData(const string& data);

  /**
   * @brief Apply FrameList to sprite
   * @param _elapsed in seconds to add to progress
   * @param target sprite to apply to
   */
  void Update(double _elapsed, sf::Sprite& target);
  
  /**
   * @brief Syncs the animation elapsed counter to one provided
   */
  void SyncTime(frame_time_t newTime);

  /**
   * @brief Same as a call to Update(0, target).
   * @param target
   */
  void Refresh(sf::Sprite& target);
  
  /**
   * @brief Manually set a frame to the sprite
   * @param frame Base 1 frame index
   * @param target Sprite to update
   * 
   * If frame is out of bounds for the current animation, ignores
   */
  void SetFrame(int frame, sf::Sprite& target);
  
  /**
   * @brief Sets the current animation from a map of FrameLists
   * @param state animation name is the map key
   */
  void SetAnimation(std::string state);

  /**
   * @brief Clears the function callbacks in the animator
   */
  void RemoveCallbacks();

  /**
   * @brief Get the current animation state name
   * @return If not animation state is set, returns empty string
   */
  const std::string GetAnimationString() const;

  /**
   * @brief Get the frame list corresponding to this animation state
   * @param animation name of the animation
   * @return FrameList&
   * @warning Make sure this animation exists otherwise could return an empty frame list or throw
   */
  FrameList& GetFrameList(std::string animation);

  /**
   * @brief Append frame callback
   * @param rhs On struct
   * @return Animation& to chain
   */
  Animation& operator<<(const Animator::On& rhs);
  
  /**
   * @brief Set the animator mode
   * @param rhs byte mode
   * @return Animation& to chain
   */
  Animation& operator<<(char rhs);
  
  /**
   * @brief Set the animation state
   * @param state name to chain
   */
  Animation& operator<<(const std::string& state);

  /**
   * @brief Set the onFinish callback
   * @param onFinish the function to call when the animation finishes
   * @warning does not return object. This must be the end of the chain.
   */
  void operator<<(const std::function<void()>& onFinish);

  sf::Vector2f GetPoint(const std::string& pointName);
  const bool HasPoint(const std::string& pointName);

  char GetMode();

  frame_time_t GetStateDuration(const std::string& state) const;

  void OverrideAnimationFrames(const std::string& animation, const std::list<OverrideFrame>& data, std::string& uuid);

  void SyncAnimation(Animation& other);

  void SetInterruptCallback(const std::function<void()> onInterrupt);

  const bool HasAnimation(const std::string& state) const;

  const double GetPlaybackSpeed() const;
  void SetPlaybackSpeed(double factor);

  /**
   * @brief Applies a callback
   * @param frame integer frame (base 1)
   * @param callback void() function callback type
   * @param doOnce boolean to fire the callback once or every time
   * @return Animation& for chaining
   *
   * This explicit function signature was added for scripting
   */
  Animation& AddCallback(int frame, FrameCallback callback, bool doOnce) {
    *this << Animator::On(frame, callback, doOnce);
    return *this;
  }

private:
  void HandleInterrupted();
protected:
  bool noAnim{ false }; /*!< If the requested state was not found, hide the sprite when updating */
  bool handlingInterrupt{ false }; /*!< Whether or not the interupt handler is executing (for nested animations) */
  Animator animator; /*!< Internal animator to delegate most of the work to */
  string path; /*!< Path to the animation file */
  string currAnimation; /*!< Name of the current animation state */
  frame_time_t progress; /*!< Current progress of animation */
  double playbackSpeed{ 1.0 }; /*!< Factor to multiply against update `dt`*/
  std::map<string, FrameList> animations; /*!< Dictionary of FrameLists read from file */
  std::function<void()> interruptCallback;
};
