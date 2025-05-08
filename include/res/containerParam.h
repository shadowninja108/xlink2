#pragma once

#include "util/types.h"

namespace xlink2 {

enum class ContainerType {
    Switch = 0,
    Random = 1,
    Random2 = 2,
    Blend = 3,
    Sequence = 4,
    
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    Grid,
#endif
#if XLINK_TARGET_IS_TOTK
    Jump,
#endif
    Mono, // not a valid type within the file - this is the type used for assets
};

struct ResContainerParam {

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    using ContainerTypePrimitive = u8;
#elif XLINK_TARGET_IS_BLITZ
    using ContainerTypePrimitive = ContainerType;
#endif

    ContainerTypePrimitive type;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    bool isNotBlendAll; // blend by default blends all child containers but this blends between two specific ones based on a value
    bool isNeedObserve; // updated at runtime, just ignore
    u8 unk; // probably just padding unless I missed where it's used
#endif

    s32 childStartIdx;
    s32 childEndIdx;

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    char padding[4];
#endif

    ContainerType getType() const {
        return static_cast<ContainerType>(type);
    }
};

struct ResSwitchContainerParam : public ResContainerParam {
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    TargetPointer actionSlotNameOffset;
    s32 _00;
    s16 propertyIndex;
    bool isGlobal;
    bool isActionTrigger; // matches by action slot name + action hash instead of by property (only Switch and not Blend)
#elif XLINK_TARGET_IS_BLITZ
    TargetPointer actionSlotNameOffset;
    s32 watchPropertyId;
    s16 propertyIndex;
    bool isGlobal;
#endif
};

struct ResRandomContainerParam : public ResContainerParam {};

struct ResRandomContainerParam2 : public ResRandomContainerParam {};

struct ResBlendContainerParam : public ResContainerParam {};


struct ResSequenceContainerParam : public ResContainerParam {};

// like a Switch but with two properties instead of one
// only for use with enums
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
struct ResGridContainerParam : public ResContainerParam {
    TargetPointer propertyNameOffset1;
    TargetPointer propertyNameOffset2;
    s16 propertyIndex1;
    s16 propertyIndex2;
    u16 flags;
    u8 propertyValueCount1;
    u8 propertyValueCount2;

    // u32 propertyValues1[propertyValueCount1];
    // u32 propertyValues2[propertyValueCount2];

    // s32 childIndices[propertyValueCount1 * propertyValueCount2];

    bool isProperty1Global() const {
        return (flags & 1) == 1;
    }

    bool isProperty2Global() const {
        return (flags >> 1 & 1) == 1;
    }
};

struct ResBlendContainerParam2 : public ResSwitchContainerParam {};
#endif

#if XLINK_TARGET_IS_TOTK
struct ResJumpContainerParam : public ResContainerParam {};
#endif

} // namespace xlink2