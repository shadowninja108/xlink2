#pragma once

#include "res/containerParam.h"
#include "res/property.h"

namespace xlink2 {

enum class BlendType {
    None = 0,
    Multiply = 1,
    SquareRoot = 2,
    Sin = 3,
    Add = 4,
    SetToOne = 5,
};

// contains conditions for the corresponding container
// condition must match the container type
struct ResCondition {
    u32 type;

    ContainerType getType() const {
        return static_cast<ContainerType>(type);
    }
};

struct ResSwitchCondition : public ResCondition {
    u8 propertyType;
    u8 compareType;
    bool solved; // internal for use at runtime, just ignore
    bool isGlobal;
    union {
        u32 actionHash;
        s32 enumValue; // index for local, value for global
    };
    union {
        s32 i;
        f32 f;
        bool b;
    } value;
    u64 enumNameOffset;

    PropertyType getPropType() const {
        return static_cast<PropertyType>(propertyType);
    }

    CompareType getCompareType() const {
        return static_cast<CompareType>(compareType);
    }
};

struct ResRandomCondition : public ResCondition {
    float weight;
};

struct ResRandomCondition2 : public ResRandomCondition {};

struct ResBlendCondition : public ResCondition {
    float min;
    float max;
    u8 blendTypeToMax;
    u8 blendTypeToMin;
    char padding[2];

    BlendType getBlendTypeToMax() const {
        return static_cast<BlendType>(blendTypeToMax);
    }

    BlendType getBlendTypeToMin() const {
        return static_cast<BlendType>(blendTypeToMin);
    }
};

struct ResSequenceCondition : public ResCondition {
    s32 isContinueOnFade;
};

struct ResGridCondition : public ResCondition {};

struct ResJumpCondition : public ResCondition {};

} // namespace xlink2