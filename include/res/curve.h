#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResCurvePoint {
    f32 x;
    f32 y;
};

struct ResCurveCallTable {
    u16 curvePointBaseIdx;
    u16 numCurvePoint;
    u16 curveType;
    u16 isGlobal;
    TargetPointer propNameOffset;
    s32 unk;
    s16 propertyIndex;
    u16 unk2;
};

} // namespace xlink2