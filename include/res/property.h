#pragma once

#include "util/types.h"

#include <string>

namespace xlink2 {

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    using CompareTypePrimitive = u8;
#else
    using CompareTypePrimitive = u32;
#endif
enum class CompareType {
    Equal = 0,
    GreaterThan = 1,
    GreaterThanOrEqual = 2,
    LessThan = 3,
    LessThanOrEqual = 4,
    NotEqual = 5,
};

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
using PropertyTypePrimitive = u8;
#else
    using PropertyTypePrimitive = u32;
#endif
enum class PropertyType {
    Enum = 0,
    S32 = 1,
    F32 = 2,
    Bool = 3,
    _04 = 4, // also integer?
    _05 = 5, // also float?
    End = 6,
};

struct ResProperty {
    TargetPointer nameOffset;
    u32 isGlobal;
    s32 triggerStartIdx;
    s32 triggerEndIdx;
};

} // namespace xlink2