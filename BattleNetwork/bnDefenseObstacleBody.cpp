#include "bnDefenseObstacleBody.h"

DefenseObstacleBody::DefenseObstacleBody() : DefenseRule(DefensePriority::Body, DefenseOrder::collisionOnly) {
}

DefenseObstacleBody::~DefenseObstacleBody() { }

Hit::Properties& DefenseObstacleBody::FilterStatuses(Hit::Properties& statuses)
{
  // obstacles are immune to most flags
  statuses.flags &= ~Hit::flash;
  statuses.flags &= ~Hit::flinch;
  statuses.flags &= ~Hit::stun;
  statuses.flags &= ~Hit::freeze;
  statuses.flags &= ~Hit::root;

  return statuses;
}

void DefenseObstacleBody::CanBlock(DefenseFrameStateJudge& judge, std::shared_ptr<Entity> attacker, std::shared_ptr<Entity> owner) {
  // doesn't block anything
}
