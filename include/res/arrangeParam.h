#pragma once

#include "util/types.h"

namespace xlink2 {

struct ArrangeGroupParams {
    u32 numGroups;
};

struct ArrangeGroupParam {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    TargetPointer groupNameOffset;
    s8 limitType;
    s8 limitThreshold;
    u8 unk;
    char padding[5];
#elif XLINK_TARGET_IS_BLITZ
    TargetPointer groupNameOffset;
    s8 limitType;
    s8 limitThreshold;
    u8 unk;
#endif
};

} // namespace xlink2