#pragma once

#include "util/types.h"

namespace xlink2 {

struct ResUserHeader {
    u32 isSetup; // internal for runtime, ignore
#if XLINK_TARGET == TOTK
    s16 localPropertyCount;
    u16 unk;
#elif XLINK_TARGET == S3
    s32 localPropertyCount;
#else
    static_assert(false, "Invalid XLINK_TARGET!");
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
    char padding[4];
    u64 triggerTableOffset;
};

} // namespace xlink2