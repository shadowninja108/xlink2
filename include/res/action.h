#pragma once

#include "util/types.h"

namespace xlink2 {

// associated with a group of ResActions
// externally, ActionSlots have an associated Action
// e.g. ActionSlot AS[0] could have the Action Crouch
struct ResActionSlot {
    TargetPointer nameOffset;
    s16 actionStartIdx;
    s16 actionEndIdx;
};

// associated with a group of ResActionTriggers
struct ResAction {

    TargetPointer nameOffset;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    using IndexType = s16;

    IndexType triggerStartIdx;
    bool enableMatchStart;
    char padding;
    IndexType triggerEndIdx; // may just be padding
#elif XLINK_TARGET_IS_BLITZ
    using IndexType = u32;
    IndexType triggerStartIdx;
    IndexType triggerEndIdx;
#endif
};

} // namespace xlink2