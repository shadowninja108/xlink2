#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResAssetCallTable {
    TargetPointer keyNameOffset;
    s16 assetIndex;
    u16 flag;
    s32 duration;
    s32 parentIndex;
    u32 guid;
    u32 keyNameHash;
    TargetPointer paramOffset;
    TargetPointer conditionOffset;

    bool isContainer() const {
        return (flag & 1) == 1;
    }
};

} // namespace xlink2