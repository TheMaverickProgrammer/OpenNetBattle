#pragma once
#include "bnScene.h"
#include "bnText.h"
#include "bnAnimatedTextBox.h"
#include "bnMessageQuestion.h"
#include "bnBackground.h"
#include <vector>

class VendorScene : public Scene {
public:
  typedef std::function<void(const std::string&)> PurchaseCallback;

  struct Item {
    std::string name;
    std::string desc;
    unsigned cost{};
  };

private:
  enum class state : char {
    slide_in = 0,
    active,
    slide_out
  } currState{};
  swoosh::Timer stateTimer{};

  class VendorBackground : public Background {
  public:
    VendorBackground();
    ~VendorBackground();

    void Update(double _elapsed) override;

  private:
    float x, y;
    sf::Sprite dummy;
  }*bg{ nullptr };

  uint32_t monies;
  double totalElapsed{};

  std::vector<Item> items;

  Text label;
  sf::Sprite wallet;
  sf::Sprite moreItems;
  sf::Sprite cursor;
  sf::Sprite list;
  std::string defaultMessage;
  std::shared_ptr<sf::Texture> mugshotTexture;
  sf::Sprite mugshot;
  Animation anim;
  AnimatedTextBox textbox;
  Question* question{ nullptr };
  Message* message{ nullptr };
  float heldCooldown{};
  signed row{}, rowOffset{};
  bool showDescription{ false }; 
  PurchaseCallback callback;

  const signed maxRows = 5;

  void ShowDefaultMessage();

public:
  VendorScene(
    swoosh::ActivityController& controller,
    const std::vector<VendorScene::Item>& items,
    uint32_t monies, 
    const std::shared_ptr<sf::Texture>& mugshotTexture,
    const Animation& anim,
    const PurchaseCallback& callback
  );

  ~VendorScene();

  void onLeave() override;
  void onExit() override;
  void onEnter() override;
  void onResume() override;
  void onStart() override;
  void onUpdate(double elapsed) override;
  void onDraw(IRenderer& renderer) override;
  void onEnd() override;
};