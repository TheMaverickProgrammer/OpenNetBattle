#pragma once

#include "../bnBattleSceneState.h"
#include "../../bnAnimatedTextBox.h"
#include <SFML/Graphics/Sprite.hpp>
#include <Swoosh/Timer.h>
/*
    \brief This state handles the retreat textbox and outcome
*/

struct RetreatBattleState final : public BattleSceneState {
  bool escaped{};
  AnimatedTextBox& textbox;
  sf::Sprite mug;
  Animation anim;

  enum class state : short {
    pending = 0,
    success,
    fail
  } currState{};

  RetreatBattleState(AnimatedTextBox& textbox, const sf::Sprite& mug, const Animation& anim);

  void onStart(const BattleSceneState*) override;
  void onUpdate(double elapsed) override;
  void onDraw(sf::RenderTexture& surface) override;
  void onEnd(const BattleSceneState*) override;
  bool Success();
  bool Fail();
};
