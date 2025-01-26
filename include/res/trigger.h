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

    u32 guid;
    u32 unk;
    u64 assetCallTableOffset;
    union {
        u64 previousActionNameOffset;
        s32 startFrame;
    };
    s32 endFrame;
    u16 flag;
    u16 overwriteHash;
    u64 overwriteParamOffset;

    bool isFlagSet(Flag f) const {
        return (flag & static_cast<u16>(f)) != 0;
    }
};

// triggered when the designated property meets the specified condition
struct ResPropertyTrigger {
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    u64 assetCallTableOffset;
    u64 conditionOffset;
    u64 overwriteParamOffset;
};

// always triggered
struct ResAlwaysTrigger {
    u32 guid;
    u16 flag;
    u16 overwriteHash;
    u64 assetCallTableOffset;
    u64 overwriteParamOffset;
};

} // namespace xlink2