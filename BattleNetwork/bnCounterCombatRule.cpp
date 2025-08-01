#include "bnCounterCombatRule.h"
#include "bnSpell.h"
#include "battlescene/bnBattleSceneBase.h"

CounterCombatRule::CounterCombatRule(BattleSceneBase* battleScene) 
  : DefenseRule(Priority(0), DefenseOrder::collisionOnly), battleScene(battleScene) {
}

CounterCombatRule::~CounterCombatRule() { }

void CounterCombatRule::CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) {
  // we lose counter ability if hit by an impact attack
  if ((attacker->GetHitboxProperties().flags & Hit::impact) == Hit::impact) {
    battleScene->HandleCounterLoss(*owner, false); // see if battle scene had blessed this character with an ability
  }
}
