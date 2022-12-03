#pragma once
#include "bnScene.h"
#include "bnText.h"
#include "bnSpriteProxyNode.h"

using namespace swoosh;

class IntroScene : public Scene {
  Text message;
  Font startFont;
  SpriteProxyNode alertSprite;
  bool inMessageState;
  float messageCooldown, speed;
public:
  IntroScene(ActivityController& controller);

  void onStart() override;
  void onUpdate(double elapsed) override;
  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onDraw(IRenderer& renderer) override;
  void onEnd() override;
};