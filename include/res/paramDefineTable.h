#pragma once

#include "res/param.h"

namespace xlink2 {

constexpr inline s32 cNumSystemELinkUserParams = 0;
constexpr inline s32 cNumSystemSLinkUserParams = 8;

struct ResParamDefine {
    u64 nameOffset;
    u32 type;
    char padding[4];
    u64 defaultValue;

    ParamType getType() const {
        return static_cast<ParamType>(type);
    }
};

struct ResParamDefineTableHeader {
    s32 size;
    s32 numUserParams;
    s32 numAssetParams;
    s32 numUserAssetParams;
    s32 numTriggerParams;
    char padding[4];

    /*
    The system reserves a certain number of user params and the remaining ones are custom user params
    SystemELink reserves 0 and SystemSLink reserves 8 (hardcoded values)
    The system also reserves a number of asset params which is just numAssetParams - numUserAssetParams
    There is a hardcoded map of asset param indices to trigger param indices so changing the order would require updating this mapping
    */
};

} // namespace xlink2