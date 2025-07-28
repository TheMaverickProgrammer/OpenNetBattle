#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <functional>
#include <assert.h>
#include <iostream>
#include <list>

#include "bnLogger.h"
#include "frame_time_t.h"

using FrameCallback = std::function<void()>;
using FrameFinishCallback = std::function<void()>;
using FrameCallbackHash = std::multimap<int, FrameCallback>;
using PointHash = std::map<std::string, sf::Vector2f>;

/**
 * @struct OverrideFrame
 * @brief a struct to override animations with using brace initialization e.g. { 3, 5.0f } */
struct OverrideFrame {
  unsigned frameIndex{};
  double duration{};
};

/**
 * @struct Frame
 * @author mav
 * @date 13/05/19
 * @brief Lightweight frame composed of  rect, duration (in seconds), an origin, points, and flipped orientations
 */
struct Frame {
  frame_time_t duration;
  sf::IntRect subregion;
  bool applyOrigin{}, flipX{}, flipY{};
  sf::Vector2f origin;

  PointHash points;

  Frame(frame_time_t duration, sf::IntRect subregion, bool applyOrigin, sf::Vector2f origin, bool flipX, bool flipY) :
    duration(duration),
    subregion(subregion),
    applyOrigin(applyOrigin),
    origin(origin),
    flipX(flipX),
    flipY(flipY)
  {
    points["ORIGIN"] = origin;
  }

  Frame(const Frame& rhs) {
    this->operator=(rhs);
  }

  Frame& operator=(const Frame& rhs) {
    duration = rhs.duration;
    subregion = rhs.subregion;
    applyOrigin = rhs.applyOrigin;
    origin = rhs.origin;
    points = rhs.points;
    flipX = rhs.flipX;
    flipY = rhs.flipY;

    return *this;
  }


  Frame& operator=(Frame&& rhs) noexcept {
    duration = rhs.duration;
    rhs.duration = {};

    subregion = rhs.subregion;

    applyOrigin = rhs.applyOrigin;
    rhs.applyOrigin = false;

    origin = rhs.origin;
    points = rhs.points;
    rhs.points.clear();

    flipX = rhs.flipX;
    flipY = rhs.flipY;

    rhs.flipX = rhs.flipY = false;

    return *this;
  }

  Frame(Frame&& rhs) noexcept {
    *this = rhs;
  }
};

/**
 * @class FrameList
 * @author mav
 * @date 13/05/19
 * @brief List of frames
 */
class FrameList {
  std::vector<Frame> frames;
  frame_time_t totalDuration; /*!< Sum of all frame durations */

public:
  friend class Animator;

  FrameList() { totalDuration = ::frames(0); }
  FrameList(const FrameList& rhs) { 
    frames = rhs.frames; 
    totalDuration = rhs.totalDuration;
  }

  FrameList MakeNewFromOverrideData(const std::list<OverrideFrame>& data) {
    FrameList res;
    if (frames.empty()) return res;

    auto iter = data.begin();
    while (iter != data.end() && data.size() > 0) {
      size_t index = static_cast<size_t>(iter->frameIndex) - 1;

      if (index >= frames.size()) {
        index = frames.size() - 1u;
      }

      Frame copy = frames[index];
      copy.duration = from_seconds(iter->duration);
      res.frames.push_back(copy);
      res.totalDuration += copy.duration;

      iter = std::next(iter);
    }

    return res;
  }

  /**
   * @brief Adds frame to list with an origin at (0,0) and increases totalDuration
   * @param dur duration of frame in seconds
   * @param sub int rectangle defining the frame from a texture sheet
   */
  inline void Add(frame_time_t dur, sf::IntRect sub) {
    frames.emplace_back(std::move(Frame(dur, sub, false, sf::Vector2f(0,0), false, false )));
    totalDuration += dur;
  }

  /**
   * @brief Adds frame to the list with a defined origin and increases totalDuration
   * @param dur duration of frame in seconds
   * @param sub int rectangle defininf the frame from a texture sheet
   * @param origin origin of frame
   * @param flipX if the frame is flipped horizontally visually (does not affect points)
   * @param flipY if the frame is flipped vertically   visually (does not affect points)
   */
  inline void Add(frame_time_t dur, sf::IntRect sub, sf::Vector2f origin, bool flipX, bool flipY) {
    frames.emplace_back(std::move(Frame(dur, sub, true, origin, flipX, flipY)));
    totalDuration += dur;
  }

  /**
  * @brief sets the (x,y) location for a point and assigns a name
  * @param name of the new point 
  * @param x of the vector
  * @param y of the vector
  * Will overwrite any other point with the same name in the frame - unique names only
  */
  inline void SetPoint(const std::string& name, int x, int y) {
    auto str = name;
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    frames[frames.size() - 1].points[name] = sf::Vector2f(float(x), float(y));
  }

  /**
 * @brief Get the total number of frames in this list
 * @return const unsigned int
 */
  inline const size_t GetFrameCount() { return frames.size(); }

  /**
  * @brief Get the frame data at the given index
  * @param index of the frame in the list (base 0)
  * @return const Frame immutable
  */
  inline const Frame& GetFrame(const int index) { return frames[index]; }

  /**
   * @brief Get the total duration for the list of frames
   * @return const float
   */
  inline const frame_time_t GetTotalDuration() const { return totalDuration; }

  /**
   * @brief Query if frame list is empty
   * @return true if empty, false otherwise
   */
  inline const bool IsEmpty() const { return frames.empty(); }
};

/**
 * @class Animator
 * @author mav
 * @date 13/05/19
 * @brief Robust animator class that handles callbacks of many kinds
 * 
 * Animator updates a frame list independantly and can be used on multiple frame lists
 * 
 * It can set callbacks on frame numbers, one time callbacks, and will intelligently
 * queue callbacks if callbacks are being added during a callback itself.
 */
class Animator {
private:
  FrameCallbackHash callbacks; /*!< Called every time on frame */
  FrameCallbackHash onetimeCallbacks; /*!< Called once on frame then discarded */
  FrameCallbackHash nextLoopCallbacks; /*!< used to queue already called callbacks */
  FrameCallbackHash queuedCallbacks; /*!< used for adding new callbacks while updating */
  FrameCallbackHash queuedOnetimeCallbacks; /*!< adding new one-time callbacks in update */
  
  PointHash currentPoints;
  
  FrameFinishCallback onFinish; /*!< special callback that fires when the animation is completed */
  FrameFinishCallback queuedOnFinish; /*!< Queues onFinish callback when used in the middle of update */
  
  char playbackMode; /*!< determins how to animate the frame list */
  
  bool isUpdating; /*!< Flag if in the middle of update */
  bool callbacksAreValid; /*!< Flag for queues. If false, all added callbacks are discarded. */
  bool clearLater{ false }; //!< if clearing inside a callback, wait until after callback scope ends

  void UpdateSpriteAttributes(sf::Sprite& target, const Frame& data);
  const sf::Vector2f CalculatePointData(const sf::Vector2f& point, const Frame& data);
public:
  inline static const FrameCallback NoCallback = [](){};

  /**
   * @struct On
   * @author mav
   * @date 13/05/19
   * @brief Struct to add new callbacks with. Uses base 1 frame indeces.
   */
  struct On {
    int id; /*!< Base 1 frame index */
    FrameCallback callback; /*!< Callback to queue */
    bool doOnce; /*!< If true, this is a one-time callback */

    friend class Animator;
    On(int id, FrameCallback callback, bool doOnce = false) : id(id), callback(callback), doOnce(doOnce) {
      ;
    }
    
    On(const On& rhs) {
      //std::cout << "in cpy constructor" << std::endl;
      id = rhs.id;
      callback = rhs.callback;
      doOnce = rhs.doOnce;
    }
  };

  /**
   * @struct Mode
   * @author mav
   * @date 13/05/19
   * @brief Defines playback bytes
   */
  struct Mode {
  private:
    int playback;
    
    friend class Animator;
  public:
    static constexpr char NoEffect = 0x00; /*!< Play once and stop */
    static constexpr char Loop = 0x01; /*!< When it reaches the end, restarts animation and resumes */
    static constexpr char Bounce = 0x02; /*!< When it reaches the end, reverse the animation and resume */
    static constexpr char Reverse = 0x04; /*!< Reverse the animation */

    Mode(int playback);

    ~Mode() = default;
  };

  Animator();
  Animator(const Animator& rhs);
  Animator& operator=(const Animator& rhs);
  ~Animator();

  /**
   * @brief Get the current playback mode
   * @return char
   */
  char GetMode();
  
  const sf::Vector2f GetPoint(const std::string& pointName);
  const bool HasPoint(const std::string& pointName);

  /**
   * @brief Clears all callback functors
   */
  void Clear();

  /**
   * @brief Overload the () operator to apply proper frame onto a sprite 
   * @param progress in seconds, the elapsed time in the animation
   * @param target sprite to apply frames to
   * @param sequence list of frames
   */
  void operator() (frame_time_t progress, sf::Sprite& target, FrameList& sequence);
  
  /**
   * @brief Applies a callback
   * @param rhs On struct
   * @return Animator& for chaining 
   */
  Animator& operator<< (On rhs);
  
  /**
   * @brief Applies a playback mode
   * @param rhs byte mode
   * @return Animator& for chaining
   */
  Animator& operator<< (char rhs);
  
  /**
   * @brief Sets an onFinish callback. 
   * @important Does not chain. This must come at the end.
   */
  void operator<< (FrameFinishCallback finishNotifier);

  /**
   * @brief Manually applies the frame at index
   * @param frameIndex Base 0 index to apply frame of
   * @param target sprite to apply frame to
   * @param sequence frame is pulled from list using index
   */
  void SetFrame(int frameIndex, sf::Sprite& target, FrameList& sequence);

  /**
 * @brief Updates the internal points hash from the frame list for a given frame
 * @param frameIndex Base 0 index to apply frame of
 * @param The frame list
 * 
 * Once this function is complete, the currentpoints stored inside the animator
 * is refreshed with latest data
 */
  void UpdateCurrentPoints(int frameIndex, FrameList& sequence);
};
