#pragma once

#include "res/action.h"
#include "res/arrangeParam.h"
#include "res/assetCallTable.h"
#include "res/condition.h"
#include "res/containerParam.h"
#include "res/curve.h"
#include "res/param.h"
#include "res/paramDefineTable.h"
#include "res/property.h"
#include "res/random.h"
#include "res/trigger.h"
#include "res/user.h"

namespace xlink2 {

constexpr inline u32 cResourceMagic = 0x4b4e4c58; // XLNK in little endian

struct ResourceHeader {
    u32 magic;
    u32 fileSize;
    u32 version;
    s32 numParams;
    s32 numAssetParams;
    s32 numTriggerOverwriteParams;
    u64 triggerOverwriteTablePos;
    u64 localPropertyNameRefTablePos;
    s32 numLocalPropertyNameRefs;
    s32 numLocalPropertyEnumNameRefs;
    s32 numDirectValues;
    s32 numRandom;
    s32 numCurves;
    s32 numCurvePoints;
    u64 exRegionPos;
    s32 numUsers;
    char padding[4];
    u64 conditionTablePos;
    u64 nameTablePos;
};
static_assert(sizeof(ResourceHeader) == 0x60);

} // namespace xlink2