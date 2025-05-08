#pragma once

#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>

namespace banana {

struct SwitchContainerParam {
    std::string_view actionSlotName;
    s16 propertyIndex;
    bool isGlobal;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    s32 unk;
    bool isActionTrigger;
#else
    s32 watchPropertyId;
#endif
};

struct RandomContainerParam {};

struct RandomContainerParam2 {};

struct BlendContainerParam {};

struct SequenceContainerParam {};

#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
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

struct BlendContainerParam2 : public SwitchContainerParam {};
#endif
#if XLINK_TARGET_IS_TOTK
struct JumpContainerParam {};
#endif

struct Container {
    xlink2::ContainerType type;
    s32 childContainerStartIdx;
    s32 childCount;
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    bool isNotBlendAll;
    bool isNeedObserve;
#endif

    template <xlink2::ContainerType T, bool Blend2 = false>
    struct ContainerType {};
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
#define DECL(enu, type, blend2)                                                     \
    template<>                                                                      \
    struct ContainerType<xlink2::ContainerType::enu, blend2> { using Type = type; }
#else
#define DECL(enu, type, blend2)                                                     \
    template<>                                                                      \
    struct ContainerType<xlink2::ContainerType::enu> { using Type = type; }
#endif


    DECL(Switch,    SwitchContainerParam, false);
    DECL(Random2,   RandomContainerParam, false);
    DECL(Sequence,  SequenceContainerParam, false);
    DECL(Blend,     BlendContainerParam, false);
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
    DECL(Blend2,    BlendContainerParam2, true);
    DECL(Grid,      GridContainerParam, false);
#endif
#if XLINK_TARGET_IS_TOTK
    DECL(Jump,      JumpContainerParam, false);
#endif
#undef DECL

    template<xlink2::ContainerType T, bool Blend2 = false>
    using ContainerTypeT = typename ContainerType<T, Blend2>::Type;

    template <xlink2::ContainerType T, bool Blend2 = false>
    auto* getAs() {
#if XLINK_TARGET != XLINK_TARGET_TOTK && XLINK_TARGET != XLINK_TARGET_THUNDER
        static_assert(Blend2 == false, "Blend2 containers are only supported on Totk and Thunder");
#endif
        return reinterpret_cast<ContainerTypeT<T, Blend2>*>(_buf);
    }

    template <xlink2::ContainerType T, bool Blend2 = false>
    const auto* getAs() const {
#if XLINK_TARGET != XLINK_TARGET_TOTK && XLINK_TARGET != XLINK_TARGET_THUNDER
        static_assert(Blend2 == false, "Blend2 containers are only supported on Totk and Thunder");
#endif
        return reinterpret_cast<std::add_const_t<ContainerTypeT<T, Blend2>>*>(_buf);
    }

private:
    
    static constexpr size_t ContainerSizes[] = {
        sizeof(SwitchContainerParam),
        sizeof(RandomContainerParam),
        sizeof(RandomContainerParam),
        sizeof(BlendContainerParam),
        sizeof(SequenceContainerParam),
#if XLINK_TARGET_IS_TOTK || XLINK_TARGET_IS_THUNDER
        sizeof(JumpContainerParam),
        sizeof(BlendContainerParam2),
#endif
#if XLINK_TARGET_IS_TOTK
        sizeof(GridContainerParam),
#endif
    };
    static constexpr size_t ContainerSize = *std::max_element(std::begin(ContainerSizes), std::end(ContainerSizes));


    alignas(void*) u8 _buf[ContainerSize];
};

} // namespace banana