#pragma once
#include "Swoosh/Ease.h"
#include "Swoosh/Activity.h"

#include "bnFolderScene.h"
#include "bnOverworldMap.h"
#include "bnInfiniteMap.h"
#include "bnSelectNaviScene.h"
#include "bnSelectMobScene.h"
#include "bnMemory.h"
#include "bnCamera.h"
#include "bnInputManager.h"
#include "bnAudioResourceManager.h"
#include "bnShaderResourceManager.h"
#include "bnTextureResourceManager.h"
#include "bnNaviRegistration.h"
#include "bnEngine.h"
#include "bnAnimation.h"
#include "bnLanBackground.h"
#include <SFML/Graphics.hpp>
#include <time.h>

class FolderScene : public swoosh::Activity {
private:
  Camera camera;

  // Menu name font
  sf::Font* font;
  sf::Text* menuLabel;

  // Selection input delays
  double maxSelectInputCooldown; // half of a second
  double selectInputCooldown;

  // Chip UI font
  sf::Font *chipFont;
  sf::Text *chipLabel;

  sf::Font *numberFont;
  sf::Text *numberLabel;

  // Chip description font
  sf::Font *chipDescFont;
  sf::Text* chipDesc;

  // folder menu graphic
  sf::Sprite bg;
  sf::Sprite folderDock;
  sf::Sprite scrollbar;
  sf::Sprite stars;
  sf::Sprite chipHolder;
  sf::Sprite element;

  // Current chip graphic
  sf::Sprite chip;
  sf::IntRect cardSubFrame;

  sf::Sprite chipIcon;
  swoosh::Timer chipRevealTimer;

  int maxChipsOnScreen;
  int currChipIndex;
  int numOfChips;

  bool gotoNextScene;

public:
  std::string FormatChipDesc(const std::string&& desc);

  virtual void onStart();
  virtual void onUpdate(double elapsed);
  virtual void onLeave();
  virtual void onExit();
  virtual void onEnter();
  virtual void onResume();
  virtual void onDraw(sf::RenderTexture& surface);
  virtual void onEnd();

  FolderScene(swoosh::ActivityController&);
  virtual ~FolderScene();
};
