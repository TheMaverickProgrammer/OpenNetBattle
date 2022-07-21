#include "bnArtifact.h"

Artifact::Artifact() : Entity() {
  SetTeam(Team::unknown);
  EnableHitbox(false);
  SetFloatShoe(true);
  SetAirShoe(true);
}
