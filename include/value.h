#pragma once

#include "resource.h"

#include <string>
#include <vector>

namespace banana {

struct CurvePoint {
    float x, y;
};

struct Curve {
    std::vector<CurvePoint> points;
    std::string propertyName;
    s16 propertyIndex;
    u16 type;
    s32 unk;
    u16 unk2;
    bool isGlobal;
};

struct Random {
    float min, max;
};

} // namespace banana