#pragma once

#include "../bnBattleSceneState.h"
#include "../../bnAnimation.h"

#include <memory>
#include <SFML/Graphics/Sprite.hpp>

class Player;

/*
    \brief This state handles transformations
*/
class CharacterTransformBattleState final : public BattleSceneState {
public:
  enum class state : int {
    fadein = 0,
    animate,
    fadeout
  } currState{ state::fadein };

private:
  double backdropInc{ 4.25 }; //!< alpha increase per frame (max 255)
  double frameElapsed{ 0 };
  bool skipBackdrop{ false };
  sf::Sprite shine;
  std::vector<Animation> shineAnimations;
  int counter = 0;
  const bool FadeInBackdrop();
  const bool FadeOutBackdrop();
  void UpdateAnimation(double elapsed);

public:
  void SkipBackdrop();
  bool IsFinished();
  void onStart(const BattleSceneState* last) override;
  void onUpdate(double elapsed) override;
  void onEnd(const BattleSceneState* next) override;
  void onDraw(sf::RenderTexture&);

  CharacterTransformBattleState();
};