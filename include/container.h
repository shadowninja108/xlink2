#pragma once

#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>

namespace banana {

struct SwitchContainerParam {
    std::string_view actionSlotName;
    s32 unk;
    s16 propertyIndex;
    bool isGlobal;
    bool isActionTrigger;
};

struct RandomContainerParam {};

struct RandomContainerParam2 {};

struct BlendContainerParam {};

struct BlendContainerParam2 : public SwitchContainerParam {};

struct SequenceContainerParam {};

struct GridContainerParam {
    // this container works by taking the two values of the two specified properties and finding the index of each value in the array
    // then using index_1 * prop_2_value_count + prop_2_value_count to index the asset call index array
    std::string_view propertyName1;
    std::string_view propertyName2;
    s16 propertyIndex1;
    s16 propertyIndex2;
    bool isGlobal1;
    bool isGlobal2;
    std::vector<u32> values1;
    std::vector<u32> values2;
    std::vector<s32> indices; // could do a 2D vector here but whatever
};

struct JumpContainerParam {};

struct Container {
    xlink2::ContainerType type;
    s32 childContainerStartIdx;
    s32 childCount;
    bool isNotBlendAll;
    bool isNeedObserve;

    template <xlink2::ContainerType T, bool Blend2 = false>
    auto* getAs() {
        if constexpr (T == xlink2::ContainerType::Switch) {
            return reinterpret_cast<SwitchContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random) {
            return reinterpret_cast<RandomContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random2) {
            return reinterpret_cast<RandomContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend && !Blend2) {
            return reinterpret_cast<BlendContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend && Blend2) {
            return reinterpret_cast<BlendContainerParam2*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Sequence) {
            return reinterpret_cast<SequenceContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Grid) {
            return reinterpret_cast<GridContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Jump) {
            return reinterpret_cast<JumpContainerParam*>(_buf);
        }
    }

    template <xlink2::ContainerType T, bool Blend2 = false>
    const auto* getAs() const {
        if constexpr (T == xlink2::ContainerType::Switch) {
            return reinterpret_cast<const SwitchContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random) {
            return reinterpret_cast<const RandomContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Random2) {
            return reinterpret_cast<const RandomContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend && !Blend2) {
            return reinterpret_cast<const BlendContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Blend && Blend2) {
            return reinterpret_cast<const BlendContainerParam2*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Sequence) {
            return reinterpret_cast<const SequenceContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Grid) {
            return reinterpret_cast<const GridContainerParam*>(_buf);
        }
        if constexpr (T == xlink2::ContainerType::Jump) {
            return reinterpret_cast<const JumpContainerParam*>(_buf);
        }
    }

private:
    alignas(void*) u8 _buf[std::max(sizeof(GridContainerParam), sizeof(SwitchContainerParam))];
};

} // namespace banana