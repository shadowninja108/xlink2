#pragma once

#include "util/types.h"

#include <string>

namespace banana {

struct ActionTrigger {
    u32 guid;
    u32 unk;
    bool triggerOnce;
    bool fade;
    bool alwaysTrigger;
    bool nameMatch;
    s32 assetCallIdx;
    s32 startFrame;
    s32 endFrame;
    s32 triggerOverwriteIdx;
    u16 overwriteHash;
    std::string_view previousActionName;
};

struct Action {
    std::string_view actionName;
    s16 actionTriggerStartIdx;
    s16 actionTriggerCount;
    bool enableMatchStart;
};

struct ActionSlot {
    std::string_view actionSlotName;
    s16 actionStartIdx;
    s16 actionCount;
};

} // namespace banana