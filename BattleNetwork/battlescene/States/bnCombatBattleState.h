#pragma once

#include "../bnBattleSceneState.h"
#include "../../bnSpriteProxyNode.h"
#include "../../bnCardUseListener.h"

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/Shader.hpp>

class Mob;
class Player;

namespace Battle {
  class Card;
}

/*
    \brief This state will govern combat rules
*/
struct CombatBattleState final : public BattleSceneState, public CardActionUseListener {
  bool canPause{ true };
  bool isPaused{ false };
  bool hasTimeFreeze{ false };
  bool clearedMob{ false };
  sf::Sprite pause;
  sf::Sprite doubleDelete;
  sf::Sprite tripleDelete;
  sf::Sprite counterHit;
  sf::Shader* pauseShader; /*!< Dim screen */
  std::vector<const BattleSceneState*> subcombatStates;
  const bool IsMobCleared() const;
  const bool HasTimeFreeze() const;
  const bool RedTeamWon() const;
  const bool BlueTeamWon() const;
  const bool PlayerDeleted() const;
  const bool PlayerRequestCardSelect();
  const bool HandleNextRoundSetup(const BattleSceneState* state);
  const bool IsStateCombat(const BattleSceneState* state);

  void EnablePausing(bool enable);
  void onStart(const BattleSceneState* last) override;
  void onEnd(const BattleSceneState* next) override;
  void onUpdate(double elapsed) override;
  void onDraw(IRenderer& renderer) override;
  void OnCardActionUsed(std::shared_ptr<CardAction> action, uint64_t timestamp) override;

  CombatBattleState(double customDuration);
};