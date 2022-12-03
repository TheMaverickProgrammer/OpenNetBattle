#pragma once
#include "bnScene.h"
#include "bnText.h"
#include "bnTextArea.h"

#include <vector>

class KeyItemScene : public Scene {
public:
  struct Item {
    std::string id;
    std::string name;
    std::string desc;
  };

private:
  Text label;
  sf::Sprite moreText;
  sf::Sprite scroll;
  sf::Sprite cursor;
  sf::Sprite bg;
  TextArea textbox;
  float totalElapsed{};
  signed col{}, row{}, rowOffset{};
  float heldCooldown{};
  std::vector<Item> items;

  const signed maxRows = 4;
  const signed maxCols = 2;
public:
  KeyItemScene(swoosh::ActivityController& controller, const std::vector<Item>& items);
  ~KeyItemScene();

  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onStart() override;
  void onUpdate(double elapsed) override;
  void onDraw(IRenderer& renderer) override;
  void onEnd() override;
};