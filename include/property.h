#pragma once

#include "resource.h"

#include <string>

namespace banana {

struct PropertyTrigger {
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    s32 assetCallTableIdx;
    s32 conditionIdx;
    s32 triggerOverwriteIdx;
};

struct Property {
    std::string_view propertyName;
    s32 propTriggerStartIdx;
    s32 propTriggerCount;
    bool isGlobal;
};

} // namespace banana