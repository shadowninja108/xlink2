#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResUserHeader {
    u32 isSetup; // internal for runtime, ignore
#if XLINK_TARGET_IS_TOTK
    s16 localPropertyCount;
    u16 unk;
#else
    s32 localPropertyCount;
#endif
    s32 callCount;
    s32 assetCount;
    s32 randomContainerCount;
    s32 actionSlotCount;
    s32 actionCount;
    s32 actionTriggerCount;
    s32 propertyCount;
    s32 propertyTriggerCount;
    s32 alwaysTriggerCount;
    TargetPointer triggerTableOffset;
};

} // namespace xlink2