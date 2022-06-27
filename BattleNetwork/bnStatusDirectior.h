#pragma once

#include "bnHitProperties.h"
#include "bnFrameTimeUtils.h"

struct AppliedStatus {
    Hit::Flags statusFlag;
    frame_time_t remainingTime;
};

class StatusBehaviorDirector :
    public AppliedStatus
{
public:
    StatusBehaviorDirector();
    virtual ~StatusBehaviorDirector();
    void SetNextStatus(Hit::Flags statusFlag, frame_time_t maxCooldown, bool deffer);
    AppliedStatus GetStatus(bool isPrevious);
    AppliedStatus previousStatus{ Hit::none, frames(0) };
    AppliedStatus currentStatus{ Hit::none, frames(0) };
};