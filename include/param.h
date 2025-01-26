#pragma once

#include "resource.h"

namespace banana {

struct Param {
    xlink2::ValueReferenceType type;
    u32 value;
    s32 index;
};

struct ParamSet {
    std::vector<Param> params;
};

} // namespace banana