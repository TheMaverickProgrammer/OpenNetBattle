#include "bnDefenseVirusBody.h"

DefenseVirusBody::DefenseVirusBody() : DefenseRule(Priority(1), DefenseOrder::collisionOnly) {
}

DefenseVirusBody::~DefenseVirusBody() { }

Hit::Properties& DefenseVirusBody::FilterStatuses(Hit::Properties& statuses)
{
  // This is all this defense rule does is filter out flash and flinch flags
  statuses.flags &= ~Hit::flash;
  statuses.flags &= ~Hit::flinch;

  return statuses;
}

void DefenseVirusBody::CanBlock(DefenseFrameStateJudge& judge, Spell& in, Character& owner) {
  // doesn't block anything
}
