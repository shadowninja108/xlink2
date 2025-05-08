#pragma once

#include "resource.h"

#include <string>

namespace banana {

struct SwitchCondition {
    xlink2::PropertyType propType;
    xlink2::CompareType compareType;
    bool isGlobal;
    union {
        u32 actionHash;
        s32 enumValue; // index for local, value for global
    };
    union {
        s32 i;
        f32 f;
        bool b;
    } conditionValue;
    std::string_view enumName; // enum class name, not value name
};

struct RandomCondition {
    float weight;
};

struct RandomCondition2 {
    float weight;
};

struct BlendCondition {
    float min, max;
    xlink2::BlendType blendTypeToMin, blendTypeToMax;
};

struct SequenceCondition {
    s32 continueOnFade;
};

struct GridCondition {};

struct JumpCondition {};



struct Condition {
    xlink2::ContainerType parentContainerType;

    template<xlink2::ContainerType T>
    struct ContainerType {};

#define DECL(enu, type) \
    template<>          \
    struct ContainerType<xlink2::ContainerType::enu> { using Type = type; }

    DECL(Switch,    SwitchCondition);
    DECL(Random,    RandomCondition);
    DECL(Random2,   RandomCondition);
    DECL(Blend,     BlendCondition);
    DECL(Sequence,  SequenceCondition);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    DECL(Grid, GridCondition);
#endif
#if XLINK_TARGET_IS_TOTK
    DECL(Jump, JumpCondition);
#endif
#undef DECL

    template<xlink2::ContainerType T>
    using ContainerTypeT = typename ContainerType<T>::Type;

    template <xlink2::ContainerType T>
    auto* getAs() {
        return reinterpret_cast<ContainerTypeT<T>*>(_buf);
    }

    template <xlink2::ContainerType T>
    const auto* getAs() const {
        return reinterpret_cast<std::add_const_t<ContainerTypeT<T>>*>(_buf);
    }

private:
    alignas(void*) u8 _buf[sizeof(SwitchCondition)] {};
};

} // namespace banana