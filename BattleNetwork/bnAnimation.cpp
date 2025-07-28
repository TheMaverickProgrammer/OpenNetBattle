#include <SFML/Graphics.hpp>
using sf::Sprite;
using sf::IntRect;

#include "bnAnimation.h"
#include "bnFileUtil.h"
#include "bnLogger.h"
#include "bnEntity.h"
#include <cmath>
#include <chrono>
#include <string_view>
#include <cstdlib>

Animation::Animation() : animator(), path("") {
  progress = frames(0);
}

Animation::Animation(const char* _path) : animator(), path(std::string(_path)) {
  Reload();
}

Animation::Animation(const string& _path) : animator(), path(_path) {
  Reload();
}

Animation::Animation(const Animation& rhs) {
  *this = rhs;
}

Animation & Animation::operator=(const Animation & rhs)
{
  noAnim = rhs.noAnim;
  animations = rhs.animations;
  animator = rhs.animator;
  currAnimation = rhs.currAnimation;
  path = rhs.path;
  progress = rhs.progress;
  return *this;
}

Animation::~Animation() {
}

void Animation::CopyFrom(const Animation& rhs)
{
  *this = rhs;
}

void Animation::Reload() {
  if (path != "") {
    string data = FileUtil::Read(path);
    LoadWithData(data);
  }
}

void Animation::Load(const std::string& newPath)
{
  if (!newPath.empty()) {
    path = newPath;
  }
  Reload();
}

static bool StartsWith(std::string_view view, std::string_view find)
{
  return view.rfind(find, 0) != std::string::npos;
}

static std::string_view GetLineWithoutComment(std::string_view view, size_t start, size_t end)
{
  std::string_view line = view.substr(start, end - start);
  auto commentStart = line.find('#', 0);

  if (commentStart != string::npos) {
    line = line.substr(0, commentStart);
  }

  return line;
}

static size_t GetNextCharPos(std::string_view view, size_t start) {
  auto nextCharPtr = std::find_if(view.begin() + start, view.end(), [](char c) { return c != ' '; });
  return nextCharPtr == view.end() ? std::string::npos : std::distance(view.begin(), nextCharPtr);
}

static std::string_view GetValue(std::string_view line, std::string_view key) {
  size_t keySearchStart = 0;

  while (keySearchStart < line.size()) {
    auto keyPos = line.find(key, keySearchStart);

    if (keyPos == std::string::npos) {
      // failed to find the key
      break;
    }

    size_t posAfterKey = keyPos + key.size();

    if (keyPos > 0 && line[keyPos - 1] != ' ') {
      // space or start of line must be before the key
      keySearchStart += posAfterKey;
      continue;
    }

    size_t equalPos = GetNextCharPos(line, posAfterKey);

    if (equalPos == std::string::npos || line[equalPos] != '=') {
      // expecting '=' after key
      // we might be in a text value
      keySearchStart += posAfterKey;
      continue;
    }

    // search for the start of the value
    auto valueStartPos = GetNextCharPos(line, equalPos + 1);

    if (valueStartPos == std::string::npos || line[valueStartPos] != '"') {
      keySearchStart += posAfterKey;
      // expected " after =
      continue;
    }

    valueStartPos += 1; // trim "
    auto valueEndPos = line.find('"', valueStartPos);

    if (valueEndPos == std::string::npos) {
      // failed to find matching "
      keySearchStart += posAfterKey;
      continue;
    }

    return line.substr(valueStartPos, valueEndPos - valueStartPos);
  }

  return "";
}

static int GetIntValue(std::string_view line, std::string_view key) {
  std::string_view valueView = GetValue(line, key);
  return (int)std::strtol(valueView.data(), nullptr, 10);
}

static float GetFloatValue(std::string_view line, std::string_view key) {
  std::string_view valueView = GetValue(line, key);
  return std::strtof(valueView.data(), nullptr);
}

static bool GetBoolValue(std::string_view line, std::string_view key) {
  std::string_view valueView = GetValue(line, key);
  return valueView == "1" || valueView == "true";
}

void Animation::LoadWithData(const string& data)
{
  int frameAnimationIndex = -1;
  vector<FrameList> frameLists;
  string currentState = "";
  frame_time_t currentAnimationDuration = frames(0);
  int currentWidth = 0;
  int currentHeight = 0;
  bool legacySupport = false;
  progress = frames(0);

  std::string_view dataView = data;
  size_t endLine = 0;
  int lineNumber = 0;

  do {
    lineNumber += 1;
    size_t startLine = endLine;
    endLine = dataView.find("\n", startLine);

    if (endLine == string::npos) {
      endLine = dataView.size();
    }

    std::string_view line = GetLineWithoutComment(dataView, startLine, endLine);
    endLine += 1;

    // NOTE: Support older animation files until we upgrade completely...
    if (StartsWith(line, "VERSION")) {
      std::string_view version = GetValue(line, "VERSION");

      if (version == "1.0") {
        legacySupport = true;
      }
    }
    else if (StartsWith(line, "imagePath")) {
      // no-op 
      // editor only at this time
      continue;
    }
    else if (StartsWith(line, "animation")) {
      if (!frameLists.empty()) {
        //std::cout << "animation total seconds: " << sf::seconds(currentAnimationDuration).asSeconds() << "\n";
        //std::cout << "animation name push " << currentState << endl;

        std::transform(currentState.begin(), currentState.end(), currentState.begin(), ::toupper);

        animations.insert(std::make_pair(currentState, frameLists.at(frameAnimationIndex)));
        currentAnimationDuration = frames(0);
      }
      currentState = GetValue(line, "state");

      if (legacySupport) {
        currentWidth = GetIntValue(line, "width");
        currentHeight = GetIntValue(line, "height");
      }

      frameLists.push_back(FrameList());
      frameAnimationIndex++;
    }
    else if (StartsWith(line, "blank")) {
      if (frameAnimationIndex == -1) {
        Logger::Logf(LogLevel::critical, "%s:%d: frame defined outside of animation state!", path.c_str(), lineNumber);
        continue;
      }

      float duration = GetFloatValue(line, "duration");

      // prevent negative frame numbers
      frame_time_t currentFrameDuration = from_seconds(std::fabs(duration));

      frameLists.at(frameAnimationIndex).Add(currentFrameDuration, IntRect{}, sf::Vector2f{ 0, 0 }, false, false);
    }
    else if (StartsWith(line, "frame")) {
      if (frameAnimationIndex == -1) {
        Logger::Logf(LogLevel::critical, "%s:%d: frame defined outside of animation state!", path.c_str(), lineNumber);
        continue;
      }

      float duration = GetFloatValue(line, "duration");

      // prevent negative frame numbers
      frame_time_t currentFrameDuration = from_seconds(std::fabs(duration));

      int currentStartx = 0;
      int currentStarty = 0;
      float originX = 0;
      float originY = 0;
      bool flipX = false;
      bool flipY = false;

      if (legacySupport) {
        currentStartx = GetIntValue(line, "startx");
        currentStarty = GetIntValue(line, "starty");
      }
      else {
        currentStartx = GetIntValue(line, "x");
        currentStarty = GetIntValue(line, "y");
        currentWidth = GetIntValue(line, "w");
        currentHeight = GetIntValue(line, "h");
        originX = (float)GetIntValue(line, "originx");
        originY = (float)GetIntValue(line, "originy");
        flipX = GetBoolValue(line, "flipx");
        flipY = GetBoolValue(line, "flipy");
      }

      currentAnimationDuration += currentFrameDuration;

      if (legacySupport) {
        frameLists.at(frameAnimationIndex).Add(currentFrameDuration, IntRect(currentStartx, currentStarty, currentWidth, currentHeight));
      }
      else {
        frameLists.at(frameAnimationIndex).Add(
          currentFrameDuration, 
          IntRect(currentStartx, currentStarty, currentWidth, currentHeight), 
          sf::Vector2f(originX, originY),
          flipX,
          flipY
        );
      }
    }
    else if (StartsWith(line, "point")) {
      if (frameAnimationIndex == -1) {
        Logger::Logf(LogLevel::critical, "%s:%d: frame defined outside of animation state!", path.c_str(), lineNumber);
        continue;
      }

      std::string pointName = std::string(GetValue(line, "label"));
      int x = GetIntValue(line, "x");
      int y = GetIntValue(line, "y");

      std::transform(pointName.begin(), pointName.end(), pointName.begin(), ::toupper);

      frameLists[frameAnimationIndex].SetPoint(pointName, x, y);
    }

  } while (endLine < data.length());

  // One more addAnimation to do if file is good
  if (frameAnimationIndex >= 0) {
    std::transform(currentState.begin(), currentState.end(), currentState.begin(), ::toupper);
    animations.insert(std::make_pair(currentState, frameLists.at(frameAnimationIndex)));
  }
}

void Animation::HandleInterrupted()
{
  if (handlingInterrupt) return;
  handlingInterrupt = true;

  if (interruptCallback && progress < animations[currAnimation].GetTotalDuration()) {
    interruptCallback();
    interruptCallback = nullptr;
  }

  handlingInterrupt = false;
}

void Animation::Refresh(sf::Sprite& target) {
  Update(0, target);
}

void Animation::Update(double elapsed, sf::Sprite& target) {
  progress += frames(std::ceil(elapsed * (float)std::fabs(playbackSpeed)));

  std::string stateNow = currAnimation;

  if (noAnim == false) {
    animator(progress, target, animations[currAnimation]);
  }
  else {
    // effectively hide
    target.setTextureRect(sf::IntRect(0, 0, 0, 0));
  }

  if(currAnimation != stateNow) {
    // it was changed during a callback
    // apply new state to target on same frame
    animator(frames(0), target, animations[currAnimation]);
    progress = frames(0);
    
    HandleInterrupted();
  }

  const frame_time_t duration = animations[currAnimation].GetTotalDuration();

  if(duration <= frames(0)) return;

  // Since we are manually keeping track of the progress, we must account for the animator's loop mode
  if (progress > duration && (animator.GetMode() & Animator::Mode::Loop) == Animator::Mode::Loop) {
    progress = frames(std::fmod(progress.count(), duration.count()));
  }
}

void Animation::SyncTime(frame_time_t newTime)
{
  progress = newTime;

  const frame_time_t duration = animations[currAnimation].GetTotalDuration();

  if (duration <= frames(0)) return;

  // Since we are manually keeping track of the progress, we must account for the animator's loop mode
  while (progress > duration && (animator.GetMode() & Animator::Mode::Loop) == Animator::Mode::Loop) {
    progress -= duration;
  }
}

void Animation::SetFrame(int frame, sf::Sprite& target)
{
  if(path.empty() || animations.empty() || animations.find(currAnimation) == animations.end()) return;

  auto size = animations[currAnimation].GetFrameCount();

  if (frame <= 0 || frame > size) {
    progress = frames(0);
    animator.SetFrame(int(size), target, animations[currAnimation]);

  }
  else {
    animator.SetFrame(frame, target, animations[currAnimation]);
    progress = frames(0);

    while (frame) {
      progress += animations[currAnimation].GetFrame(--frame).duration;
    }
  }
}

void Animation::SetAnimation(string state) {
  HandleInterrupted();
  RemoveCallbacks();
  progress = frames(0);

  std::transform(state.begin(), state.end(), state.begin(), ::toupper);

  auto pos = animations.find(state);

  noAnim = false; // presumptious reset

  if (pos == animations.end()) {
#ifdef BN_LOG_MISSING_STATE
    Logger::Log("No animation found in file for \"" + state + "\"");
#endif
    noAnim = true;
  }
  else {
    animator.UpdateCurrentPoints(0, pos->second);
  }

  // Even if we don't have this animation, switch to it anyway
  currAnimation = state;
}

void Animation::RemoveCallbacks()
{
  animator.Clear();
  interruptCallback = nullptr;
}

const std::string Animation::GetAnimationString() const
{
  return currAnimation;
}

FrameList & Animation::GetFrameList(std::string animation)
{
  std::transform(animation.begin(), animation.end(), animation.begin(), ::toupper);
  return animations[animation];
}

Animation & Animation::operator<<(const Animator::On& rhs)
{
  animator << rhs;
  return *this;
}

Animation & Animation::operator<<(char rhs)
{
  animator << rhs;
  return *this;
}

Animation& Animation::operator<<(const std::string& state) {
  SetAnimation(state);
  return *this;
}

void Animation::operator<<(const std::function<void()>& onFinish)
{
  animator << onFinish;
}

sf::Vector2f Animation::GetPoint(const std::string & pointName)
{
  std::string point = pointName;
  std::transform(point.begin(), point.end(), point.begin(), ::toupper);

  const sf::Vector2f res = animator.GetPoint(point);

  return res;
}

const bool Animation::HasPoint(const std::string& pointName)
{
  return animator.HasPoint(pointName);
}

char Animation::GetMode()
{
  return animator.GetMode();
}

frame_time_t Animation::GetStateDuration(const std::string& state) const
{
  auto iter = animations.find(state);
  
  if (iter != animations.end()) {
    return iter->second.GetTotalDuration();
  }
  
  return frames(0);
}

void Animation::OverrideAnimationFrames(const std::string& animation, const std::list<OverrideFrame>&data, std::string& uuid)
{
  std::string currentAnimation = animation;
  std::transform(currentAnimation.begin(), currentAnimation.end(), currentAnimation.begin(), ::toupper);

  if (uuid.empty()) {
    uuid = animation + "@" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  }

  if (animations.find(uuid) != animations.end()) return;

  animations.emplace(uuid, std::move(animations[animation].MakeNewFromOverrideData(data)));
}

void Animation::SyncAnimation(Animation& other)
{
  other.progress = progress;
  other.currAnimation = currAnimation;
}

void Animation::SetInterruptCallback(const std::function<void()> onInterrupt)
{
  interruptCallback = onInterrupt;
}

const bool Animation::HasAnimation(const std::string& state) const
{
  return animations.find(state) != animations.end();
}

const double Animation::GetPlaybackSpeed() const
{
  return playbackSpeed;
}

void Animation::SetPlaybackSpeed(double factor)
{
  playbackSpeed = factor;
}
