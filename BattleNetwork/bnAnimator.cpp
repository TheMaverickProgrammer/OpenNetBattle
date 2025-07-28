#include "bnAnimator.h"

#include <iostream>

Animator::Mode::Mode(int playback)
{
  Mode::playback = playback;
}


void Animator::UpdateSpriteAttributes(sf::Sprite& target, const Frame& data)
{
  // rect attr 
  int x = data.subregion.left;
  int y = data.subregion.top;
  int w = data.subregion.width;
  int h = data.subregion.height;

  float originx = static_cast<float>(data.origin.x);
  float originy = static_cast<float>(data.origin.y);

  // flip x and flip y attr
  if (data.flipX) {
    originx = w - originx;

    float newX = static_cast<float>(x + w);
    w = -w;
    x = static_cast<int>(newX);
  }

  if (data.flipY) {
    originy = h - originy;

    float newY = static_cast<float>(y + h);
    h = -h;
    y = static_cast<int>(newY);
  }

  target.setTextureRect(sf::IntRect(x, y, w, h));

  // origin attr
  if (data.applyOrigin) {
    target.setOrigin(originx, originy);
  }
}

const sf::Vector2f Animator::CalculatePointData(const sf::Vector2f& point, const Frame& data)
{
  // rect attr 
  int w = data.subregion.width;
  int h = data.subregion.height;

  float pointx = static_cast<float>(point.x);
  float pointy = static_cast<float>(point.y);

  // flip x and flip y attr
  if (data.flipX) {
    pointx = w - pointx;
  }

  if (data.flipY) {
    pointy = h - pointy;
  }

  return sf::Vector2f{ pointx, pointy };
}

Animator::Animator() {
  onFinish = nullptr;
  queuedOnFinish = nullptr;
  isUpdating = false;
  callbacksAreValid = true;
  playbackMode = 0x00;
}

Animator::Animator(const Animator& rhs) {
  *this = rhs;
}

Animator & Animator::operator=(const Animator & rhs)
{
  onFinish = rhs.onFinish;
  callbacks = rhs.callbacks;
  onetimeCallbacks = rhs.onetimeCallbacks;
  nextLoopCallbacks = rhs.nextLoopCallbacks;
  queuedCallbacks = rhs.queuedCallbacks;
  queuedOnetimeCallbacks = rhs.queuedOnetimeCallbacks;
  queuedOnFinish = rhs.queuedOnFinish;
  isUpdating = rhs.isUpdating;
  callbacksAreValid = rhs.callbacksAreValid;
  currentPoints = rhs.currentPoints;
  playbackMode = rhs.playbackMode;

  return *this;
}

Animator::~Animator() {
  callbacks.clear();
  queuedCallbacks.clear();
  onetimeCallbacks.clear();
  queuedOnetimeCallbacks.clear();
  nextLoopCallbacks.clear();
  onFinish = nullptr;
  queuedOnFinish = nullptr;
}

void Animator::UpdateCurrentPoints(int frameIndex, FrameList& sequence) {
  if (sequence.frames.size() <= frameIndex) return;

  auto& data = sequence.frames[frameIndex];
  currentPoints = data.points;

  for (auto& [key, point] : currentPoints) {
    point = CalculatePointData(point, data);
  }
}

void Animator::operator() (frame_time_t progress, sf::Sprite& target, FrameList& sequence) {
  frame_time_t startProgress = progress;

  // If we did not progress while in an update, do not merge the queues and ignore this request 
  // All we wish to do is re-adjust the origin if applicable
  if (progress == frames(0) && sequence.frames.size()) {
    auto iter = sequence.frames.begin();
    size_t index = 1;

    // If the playback mode is reverse, flip the frames
    // and the index
    if ((playbackMode & Mode::Reverse) == Mode::Reverse) {
      iter = sequence.frames.end()-1;
      index = sequence.frames.size();
    }

    // apply rect, flip, and origin attributes
    UpdateSpriteAttributes(target, *iter);

    // animation index are base 1
    UpdateCurrentPoints(int(index-1), sequence);
    return;
  }


  // Callbacks are only invalide during clears in the update loop
  if (!callbacksAreValid) callbacksAreValid = true;

  // Set our flag to let callback additions go to the right queue  
  isUpdating = true;

  if (sequence.frames.empty() || sequence.GetTotalDuration() == frames(0)) {
    if (onFinish != nullptr) {
      // Fire the onFinish callback if available
      onFinish();
      onFinish = nullptr;
    }

    // We've ended the update loop
    isUpdating = false;

    // All callbacks are in a valid state
    callbacksAreValid = true;

    // Insert any queued callbacks into the callback list
    callbacks.insert(queuedCallbacks.begin(), queuedCallbacks.end());
    queuedCallbacks.clear();

    // Insert any queued one-time callbacks into the one-time callback list
    onetimeCallbacks.insert(queuedOnetimeCallbacks.begin(), queuedOnetimeCallbacks.end());
    queuedOnetimeCallbacks.clear();

    // Insert any queued onFinish callback into the onFinish callback
    if (queuedOnFinish) {
      onFinish = queuedOnFinish;
      queuedOnFinish = nullptr;
    }

    // End
    return;
  }

  // We may modify the frame order, make a copy
  std::vector<Frame> copy = sequence.frames;

  // If the playback mode is reverse, flip the frames
  if ((playbackMode & Mode::Reverse) == Mode::Reverse) {
    reverse(copy.begin(), copy.end());
  }

  // frame index
  int index = 0;

  // Iterator to the frame itself
  std::vector<Frame>::const_iterator iter = copy.begin();

  // While there is time left in the progress loop
  while (progress > frames(0)) {
    // Increase the index
    index++;

    // Subtract from the progress
    progress -= (*iter).duration;

    // Must be <= and not <, to handle case (progress == frame.duration) correctly
    // We assume progress hits zero because we use it as a decrementing counter
    // We add a check to ensure the start progress wasn't also 0
    // If it did not start at zero, we know we came across the end of the animation
    bool reachedLastFrame = &(*iter) == &copy.back() && startProgress != frames(0);

    if (progress <= frames(0) || reachedLastFrame) {
      FrameCallbackHash::iterator callbackIter = callbacks.begin();
      FrameCallbackHash::iterator callbackFind = callbacks.find(index);
      FrameCallbackHash::iterator onetimeCallbackIter = onetimeCallbacks.find(index);

      // step through and execute any callbacks that haven't triggerd up to this frame
      while (callbacksAreValid && callbacks.size() && callbackIter != callbackFind && callbackFind != callbacks.end()) {
        if (callbackIter->second) {
          callbackIter->second();
        }

        // If the callback modified the first callbacks list, break
        if (!callbacksAreValid) break;

        // Otherwise add the callback into the next loop queue
        nextLoopCallbacks.insert(*callbackIter);

        // Erase the callback so we don't fire again
        callbackIter = callbacks.erase(callbackIter);

        // Find the callback at the given index b/c iterator will be invalidated
        callbackFind = callbacks.find(index);
      }

      // If callbacks are ok and the iterator matches the expected frame
      if (callbacksAreValid && callbacks.size() && callbackIter == callbackFind && callbackFind != callbacks.end()) {
        if (callbackIter->second) {
          callbackIter->second();
        }

        if (callbacksAreValid) {
          nextLoopCallbacks.insert(*callbackIter);
          callbackIter = callbacks.erase(callbackIter);
        }
      }

      if (callbacksAreValid && onetimeCallbackIter != onetimeCallbacks.end()) {
        if (onetimeCallbackIter->second) {
          onetimeCallbackIter->second();
        }

        if (callbacksAreValid) {
          onetimeCallbacks.erase(onetimeCallbackIter);
        }
      }

      // Determine if the progress has completed the animation
      // NOTE: Last frame doesn't mean all the time has been used. Check for total duration
      if (reachedLastFrame && startProgress >= sequence.totalDuration && callbacksAreValid) {
        if (onFinish != nullptr) {
          // If applicable, fire the onFinish callback
          onFinish();

          // If we do not loop the animation, empty the onFinish notifier. If we don't empty the notifier, this fires infinitely...
          if ((playbackMode & Mode::Loop) != Mode::Loop) {
            onFinish = nullptr;
          }
        }
      }

      // If the playback mode was set to loop...
      if ((playbackMode & Mode::Loop) == Mode::Loop && &(*iter) == &copy.back() && startProgress >= sequence.totalDuration) {
        // But it was also set to bounce, reverse the list and start over
        if ((playbackMode & Mode::Bounce) == Mode::Bounce) {
          reverse(copy.begin(), copy.end());
          iter = copy.begin();
          iter++;
        }
        else {
          // It was set only to loop, start from the beginning
          iter = copy.begin();
        }

        if (callbacksAreValid) {
          // Clear callbacks
          callbacks.clear();

          // Enqueue the callbacks for the next round
          callbacks = nextLoopCallbacks;
          nextLoopCallbacks.clear();

          // callbacksAreValid = true;
        }

        continue; // Start loop again
      }

      // apply rect, flip, and origin attributes
      UpdateSpriteAttributes(target, *iter);

      UpdateCurrentPoints(index - 1, sequence);

      break;
    }

    // If not finish, go to next frame
    iter++;
  }

  // If we prematurely ended the loop, update the sprite
  if (iter != copy.end()) {
    // apply rect, flip, and origin attributes
    UpdateSpriteAttributes(target, *iter);
  }

  // End updating flag
  isUpdating = false;

  if (clearLater) {
    onFinish = nullptr;
    clearLater = false;
  }

  callbacksAreValid = true;

  if (index == 0) {
    index = 1;
  }

  UpdateCurrentPoints(index - 1, sequence);

  // Merge queued callbacks
  callbacks.insert(queuedCallbacks.begin(), queuedCallbacks.end());
  queuedCallbacks.clear();

  onetimeCallbacks.insert(queuedOnetimeCallbacks.begin(), queuedOnetimeCallbacks.end());
  queuedOnetimeCallbacks.clear();

  if (queuedOnFinish) {
    onFinish = queuedOnFinish;
    queuedOnFinish = nullptr;
  }
}


/**
 * @brief Add a callback 
 * @param rhs On struct
 * @return Animator& to chain
 * 
 * If in the middle of an update loop, add to the queue otherwise add directly to callback list
 */
Animator & Animator::operator<<(On rhs)
{
  if(!rhs.callback) return *this;
  
  if (rhs.doOnce) {
    if(isUpdating) {
      queuedOnetimeCallbacks.insert(std::make_pair(rhs.id, rhs.callback));
    } else {
          onetimeCallbacks.insert(std::make_pair(rhs.id, rhs.callback));
      }
  }
  else {
  if(isUpdating) {
        queuedCallbacks.insert(std::make_pair(rhs.id, rhs.callback));
    } else {
        callbacks.insert(std::make_pair(rhs.id, rhs.callback));		
  }
  }

  return *this;
}

Animator & Animator::operator<<(char rhs)
{
  playbackMode = rhs;
  return *this;
}

void Animator::operator<<(FrameFinishCallback finishNotifier)
{
  if(!finishNotifier) return;
  
  if(!isUpdating) {
    onFinish = finishNotifier;
  } else {
    queuedOnFinish = finishNotifier;
  }
}

char Animator::GetMode()
{
  return playbackMode;
}

const sf::Vector2f Animator::GetPoint(const std::string& pointName) {
  std::string str = pointName;
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);
  if (currentPoints.find(str) == currentPoints.end()) {
#ifdef BN_LOG_MISSING_POINT
    Logger::Log("Could not find point in current sequence named " + str);
#endif
    return sf::Vector2f();
  }
  return currentPoints[str];
}

const bool Animator::HasPoint(const std::string& pointName)
{
  std::string str = pointName;
  std::transform(str.begin(), str.end(), str.begin(), ::toupper);

  if (currentPoints.find(str) == currentPoints.end()) {
    return false;
  }

  return true;
}

void Animator::Clear() {
  callbacksAreValid = false;
  queuedCallbacks.clear(); queuedOnetimeCallbacks.clear(); queuedOnFinish = nullptr;
  nextLoopCallbacks.clear(); callbacks.clear(); onetimeCallbacks.clear(); playbackMode = 0;

  if (isUpdating) {
    clearLater = true;
  }
  else {
    clearLater = false;
    onFinish = nullptr;
  }
}

void Animator::SetFrame(int frameIndex, sf::Sprite& target, FrameList& sequence)
{
  int index = 0;
  for (Frame& frame : sequence.frames) {
    index++;

    if (index == frameIndex) {
      target.setTextureRect(frame.subregion);
      if (frame.applyOrigin) {
        target.setOrigin((float)frame.origin.x, (float)frame.origin.y);
      }
      
      UpdateCurrentPoints(frameIndex-1, sequence);

      return;
    }
  }

  Logger::Log(LogLevel::debug, "finished without applying frame. Frame sizes: " + std::to_string(sequence.frames.size()));

}
