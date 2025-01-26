#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResAssetCallTable {
    u64 keyNameOffset;
    s16 assetIndex;
    u16 flag;
    s32 duration;
    s32 parentIndex;
    u32 guid;
    u32 keyNameHash;
    char padding[4];
    u64 paramOffset;
    u64 conditionOffset;

    bool isContainer() const {
        return (flag & 1) == 1;
    }
};

} // namespace xlink2