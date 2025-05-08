#pragma once

#include "util/types.h"

#include <string>

namespace banana {

struct ActionTrigger {
    u32 guid;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    u32 unk;
#endif
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
    xlink2::ResAction::IndexType actionTriggerStartIdx;
    xlink2::ResAction::IndexType actionTriggerCount;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    bool enableMatchStart;
#endif
};

struct ActionSlot {
    std::string_view actionSlotName;
    s16 actionStartIdx;
    s16 actionCount;
};

} // namespace banana