#pragma once

#include "util/types.h"

namespace xlink2 {

struct ArrangeGroupParams {
    u32 numGroups;
};

struct ArrangeGroupParam {
    u64 groupNameOffset;
    s8 limitType;
    s8 limitThreshold;
    u8 unk;
    char padding[5];
};

} // namespace xlink2