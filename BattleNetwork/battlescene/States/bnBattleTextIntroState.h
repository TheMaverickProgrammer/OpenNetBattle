#pragma once

#include "../bnBattleSceneState.h"
#include "../../bnText.h"
#include "../../frame_time_t.h"

#include <SFML/Graphics/Sprite.hpp>
#include <Swoosh/Timer.h>
/*
    \brief This state handles the battle start message that appears
*/

class Player;

struct BattleTextIntroState : public BattleSceneState {
  frame_time_t preBattleLength{ frames(60) }; 
  frame_time_t startupDelay{ frames(0) }; /*!< Animation delay*/
  Text battleStart; /*!< e.g. "Battle Start" graphic */
  frame_time_stopwatch_t timer; /*!< How long the start graphic should stay on screen */
  sf::Vector2f battleStartPos; /*!< Position of battle pre/post graphic on screen */
  BattleTextIntroState();
  virtual ~BattleTextIntroState();

  void SetStartupDelay(frame_time_t frames);
  void SetIntroText(const std::string& str);
  void onStart(const BattleSceneState* last) override;
  void onEnd(const BattleSceneState* next) override;
  void onUpdate(double elapsed) override;
  void onDraw(sf::RenderTexture& surface) override;
  bool IsFinished();
};
