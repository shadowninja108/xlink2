#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResUserHeader {
    u32 isSetup; // internal for runtime, ignore
    s16 localPropertyCount;
    u16 unk;
    s32 callCount;
    s32 assetCount;
    s32 randomContainerCount;
    s32 actionSlotCount;
    s32 actionCount;
    s32 actionTriggerCount;
    s32 propertyCount;
    s32 propertyTriggerCount;
    s32 alwaysTriggerCount;
    char padding[4];
    u64 triggerTableOffset;
};

} // namespace xlink2