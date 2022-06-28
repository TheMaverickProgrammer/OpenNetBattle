#include "bnStatusDirectior.h"

StatusBehaviorDirector::StatusBehaviorDirector()
{
    // At creation, make sure both statuses are default.
    previousStatus = AppliedStatus{ Hit::none, frames(0) };
    currentStatus = AppliedStatus{ Hit::none, frames(0) };
}

void StatusBehaviorDirector::SetNextStatus(Hit::Flags statusFlag, frame_time_t maxCooldown, bool deffer) {
    // Since we might use it twice, create it once. No need to repeat code.
    AppliedStatus statusToSet{ statusFlag, maxCooldown };
    // Set the current status. It will be used later.
    currentStatus = statusToSet;
    if (deffer) {
        // If necessary, set the previous status as well.
        previousStatus = statusToSet;
    }
};

AppliedStatus StatusBehaviorDirector::GetStatus(bool isPrevious) {
    // If the previous status is being called, grab it.
    if (isPrevious) {
        return previousStatus;
    }
    // Otherwise return the current.
    return currentStatus;
};

StatusBehaviorDirector::~StatusBehaviorDirector()
{
}
