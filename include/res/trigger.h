#pragma once

#include "util/types.h"

namespace xlink2 {

// triggers when the specified action occurs (actions are external signals)
struct ResActionTrigger {
    enum class Flag {
        TriggerOnce     = 1 << 0,
        Fade            = 1 << 2,
        AlwaysTrigger   = 1 << 3,
        NameMatch       = 1 << 4,
    };

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    u32 guid;
    u32 unk;
    TargetPointer assetCallTableOffset;
    union {
        TargetPointer previousActionNameOffset;
        s32 startFrame;
    };
    s32 endFrame;
    u16 flag;
    u16 overwriteHash;
    TargetPointer overwriteParamOffset;
#elif XLINK_TARGET_IS_BLITZ
    u32 guid;
    TargetPointer assetCallTableOffset;
    union {
        TargetPointer previousActionNameOffset;
        s32 startFrame;
    };
    s32 endFrame;
    u16 flag;
    u16 overwriteHash;
    TargetPointer overwriteParamOffset;
#endif

    bool isFlagSet(Flag f) const {
        return (flag & static_cast<u16>(f)) != 0;
    }
};

// triggered when the designated property meets the specified condition
struct ResPropertyTrigger {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    TargetPointer assetCallTableOffset;
    TargetPointer conditionOffset;
    TargetPointer overwriteParamOffset;
#elif XLINK_TARGET_IS_BLITZ
    u32 guid;
    TargetPointer assetCallTableOffset;
    TargetPointer conditionOffset;
    u16 flag;
    u16 overwriteHash;
    TargetPointer overwriteParamOffset;
#endif
};

// always triggered
struct ResAlwaysTrigger {
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    TargetPointer assetCallTableOffset;
    TargetPointer overwriteParamOffset;
};

} // namespace xlink2