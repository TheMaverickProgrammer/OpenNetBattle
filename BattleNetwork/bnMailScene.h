#pragma once
#include "bnScene.h"
#include "bnText.h"
#include "bnTextArea.h"
#include "bnInbox.h"
#include "bnAnimation.h"

#include <vector>

class MailScene : public Scene {
private:
  bool isInFocus{};
  bool isReading{};
  size_t reading{std::numeric_limits<size_t>::max()};
  Text label;
  sf::Sprite moreText;
  sf::Sprite scroll;
  sf::Sprite cursor;
  sf::Sprite bg;
  sf::Sprite newSprite;
  std::shared_ptr<sf::Texture> iconTexture, noMug;
  TextArea textbox;
  Animation iconAnim, newAnim;
  float totalElapsed{};
  size_t row{}, rowOffset{};
  float heldCooldown{};
  Inbox& inbox;

  const signed maxRows = 4;

  void ResetTextBox();
  std::string GetStringFromIcon(Inbox::Icons icon);
public:
  MailScene(swoosh::ActivityController& controller, Inbox& inbox);
  ~MailScene();

  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onStart() override;
  void onUpdate(double elapsed) override;
  void onDraw(IRenderer& renderer) override;
  void onEnd() override;
};