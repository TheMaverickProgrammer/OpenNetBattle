#pragma once
#include <string>
#include <vector>
#include "../bnInputEvent.h"
#include "../bnDirection.h"

struct NetPlayFlags {
  bool remoteChangeForm{ false };
  bool remoteConnected{ false };
  int remoteTileX{ 5 }, remoteTileY{ 2 }; // Spawn start pos hardcoded for now
  int remoteFormSelect{ -1 };
  std::string remoteNaviId;
};