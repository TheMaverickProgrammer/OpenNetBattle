#pragma once

#include "../bnBattleSceneState.h"

class BattleResultsWidget;
class Player;
class Mob;

/*
    \brief This state rewards the player
*/
struct RewardBattleState final : public BattleSceneState {

  BattleResultsWidget* battleResultsWidget{ nullptr }; /*!< modal that pops up when player wins */
  Mob* mob{ nullptr };
  int* hitCount{ nullptr };
  double elapsed{ 0 };

  RewardBattleState(Mob* mob, int* hitCount);
  ~RewardBattleState();
  void onStart(const BattleSceneState* last) override;
  void onEnd(const BattleSceneState* next) override;
  void onUpdate(double elapsed) override;
  void onDraw(IRenderer& renderer) override;
};