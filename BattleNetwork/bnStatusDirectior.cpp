#include "bnStatusDirectior.h"

StatusBehaviorDirector::StatusBehaviorDirector()
{
    previousStatus = AppliedStatus{ Hit::none, frames(0) };
    currentStatus = AppliedStatus{ Hit::none, frames(0) };
}

void StatusBehaviorDirector::SetNextStatus(Hit::Flags statusFlag, frame_time_t maxCooldown, bool deffer) {
    currentStatus = AppliedStatus{ statusFlag, maxCooldown };
    if (deffer) {
        previousStatus = AppliedStatus{ statusFlag, maxCooldown };
    }
};

AppliedStatus StatusBehaviorDirector::GetStatus(bool isPrevious) {
    if (isPrevious) {
        return previousStatus; //Everything works, but it says Do Not Slice - seems to be a warning about data management. I can't seem to crack it.
    }
    return currentStatus;
};


StatusBehaviorDirector::~StatusBehaviorDirector()
{
    //Reset both statuses to default when deleting.
    previousStatus = AppliedStatus{ Hit::none, frames(0) };
    currentStatus = AppliedStatus{ Hit::none, frames(0) };
}
