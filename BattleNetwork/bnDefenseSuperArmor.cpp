#include "bnDefenseSuperArmor.h"

DefenseSuperArmor::DefenseSuperArmor() : DefenseRule(Priority(1), DefenseOrder::collisionOnly) {
}

DefenseSuperArmor::~DefenseSuperArmor() { }

Hit::Properties& DefenseSuperArmor::FilterStatuses(Hit::Properties& statuses)
{
  // This is all this defense rule does is filter out flinch
  statuses.flags &= ~Hit::flinch;

  return statuses;
}

void DefenseSuperArmor::CanBlock(DefenseFrameStateJudge& judge, Spell& in, Character& owner) {
  // doesn't block anything
}
