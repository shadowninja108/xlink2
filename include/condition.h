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

    template <xlink2::ContainerType T>
    auto* getAs() {
        if constexpr (T == xlink2::ContainerType::Switch) {
            return reinterpret_cast<SwitchCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random) {
            return reinterpret_cast<RandomCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random2) {
            return reinterpret_cast<RandomCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend) {
            return reinterpret_cast<BlendCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Sequence) {
            return reinterpret_cast<SequenceCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Grid) {
            return reinterpret_cast<GridCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Jump) {
            return reinterpret_cast<JumpCondition*>(_buf);
        }
    }

    template <xlink2::ContainerType T>
    const auto* getAs() const {
        if constexpr (T == xlink2::ContainerType::Switch) {
            return reinterpret_cast<const SwitchCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random) {
            return reinterpret_cast<const RandomCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random2) {
            return reinterpret_cast<const RandomCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend) {
            return reinterpret_cast<const BlendCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Sequence) {
            return reinterpret_cast<const SequenceCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Grid) {
            return reinterpret_cast<const GridCondition*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Jump) {
            return reinterpret_cast<const JumpCondition*>(_buf);
        }
    }

private:
    alignas(void*) u8 _buf[sizeof(SwitchCondition)];
};

} // namespace banana