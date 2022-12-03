#pragma once
#include <memory>
#include <future>
#include "bnLoaderScene.h"
#include "bnSpriteProxyNode.h"
#include "bnFont.h"
#include "bnText.h"
#include "bnAnimatedTextBox.h"

// forward decl.
class Message;

class TitleScene final : public LoaderScene {
private:
  // Status flags
  bool leaving{};
  bool checkMods{};
  bool pressedStart{};
  unsigned progress{}, total{};
  unsigned ellipsis{};
  double totalElapsed{};
  frame_time_t fastStartFlicker{ frames(60) };
  std::string taskStr, incomingTaskStr;
  std::vector<std::string> logs;

  SpriteProxyNode logoSprite, alertSprite;
  SpriteProxyNode bgSprite;
  SpriteProxyNode progSprite;
  SpriteProxyNode cursorSprite;
  Font font, startFont;
  Text logLabel, startLabel;
  Animation anim;
  AnimatedTextBox textbox;
  Message* currMessage{ nullptr };

  void CenterLabel();
public:
  TitleScene(swoosh::ActivityController& controller, TaskGroup&& tasks);
  void onStart() override;
  void onUpdate(double elapsed) override;
  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onDraw(IRenderer& renderer) override;
  void onEnd() override;
  void onTaskBegin(const std::string& taskName, float progress) override;
  void onTaskComplete(const std::string& taskName, float progress) override;
};