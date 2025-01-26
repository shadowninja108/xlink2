#pragma once

#include "param.h"

#include <vector>

namespace banana {

struct AlwaysTrigger {
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    s32 assetCallIdx;
    s32 triggerOverwriteIdx;
};

} // namespace banana