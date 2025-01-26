#pragma once

#include "util/types.h"

namespace xlink2 {

// associated with a group of ResActions
// externally, ActionSlots have an associated Action
// e.g. ActionSlot AS[0] could have the Action Crouch
struct ResActionSlot {
    u64 nameOffset;
    s16 actionStartIdx;
    s16 actionEndIdx;
    char _padding[4];
};

// associated with a group of ResActionTriggers
struct ResAction {
    u64 nameOffset;
    s16 triggerStartIdx;
    bool enableMatchStart;
    char padding;
    u32 triggerEndIdx; // may just be padding
};

} // namespace xlink2