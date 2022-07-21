#include "bnDefenseVirusBody.h"

DefenseVirusBody::DefenseVirusBody() : DefenseRule(DefensePriority::Body, DefenseOrder::collisionOnly) {
}

DefenseVirusBody::~DefenseVirusBody() { }

Hit::Properties& DefenseVirusBody::FilterStatuses(Hit::Properties& statuses)
{
  // This is all this defense rule does is filter out flash and flinch flags
  statuses.flags &= ~Hit::flash;
  statuses.flags &= ~Hit::flinch;

  return statuses;
}

void DefenseVirusBody::CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) {
  // doesn't block anything
}
