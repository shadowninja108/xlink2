#pragma once

#include "util/types.h"

#include <string>

namespace banana {

struct AssetCallTable {
    std::string_view keyName;
    s16 assetIndex;
    u16 flag;
    s32 duration;
    s32 parentIndex;
    u32 guid;
    u32 keyNameHash;
    union {
        s32 assetParamIdx;
        s32 containerParamIdx;
    };
    s32 conditionIdx;

    bool isContainer() const {
        return (flag & 1) == 1;
    }
};

} // namespace banana