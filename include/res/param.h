#pragma once

#include "util/types.h"
#include "util/common.h"

namespace xlink2 {

enum class ParamType {
    Int = 0,
    Float = 1,
    Bool = 2,
    Enum = 3,
    String = 4,
    Bitfield = 5, // might be more akin to custom actually
};

// the Random types + Curve are only valid for floats
enum class ValueReferenceType {
    Direct = 0x0,                           // index into the DirectValueTable
    String = 0x1,                           // offset into the NameTable
    Curve = 0x2,                            // index into the ResCurveCallTable
    Random = 0x3,                           // index into the ResRandomCallTable
    ArrangeParam = 0x4,                      // offset relative to the start of ExRegion
    Bitfield = 0x5,
    RandomPowHalf2 = 0x6,
    RandomPowHalf3 = 0x7,
    RandomPowHalf4 = 0x8,
    RandomPowHalf1Point5 = 0x9,
    RandomPow2 = 0xA,
    RandomPow3 = 0xB,
    RandomPow4 = 0xC,
    RandomPow1Point5 = 0xD,
    RandomPowComplement2 = 0xE,
    RandomPowComplement3 = 0xF,
    RandomPowComplement4 = 0x10,
    RandomPowComplement1Point5 = 0x11,

    /*
    Random = MIN + RANGE * RAND(0.0, 1.0)
    RandomPowHalf = MIN + RANGE / 2 + RANGE / 2 * RAND(0.0, 1.0) ^ POW
    RandomPow = MIN + RANGE * RAND(0.0, 1.0) ^ POW
    RandomPowComplement = MIN + RANGE * (1.0 - RAND(0.0, 1.0) ^ POW)
    */
};

struct ResParam {
    u32 value;

    ValueReferenceType getValueReferenceType() const {
        return static_cast<ValueReferenceType>(value >> 0x18);
    }

    u32 getValue() const {
        return value & 0xffffff;
    }
};

struct ResTriggerOverwriteParam {
    u32 values;

    bool hasResParam(u32 type) const {
        return (values >> type & 1) == 1;
    }

    s32 getResParamIndex(u32 type) const {
        if (!hasResParam(type))
            return -1;

        return ::util::countOnBit(values, type) - 1;
    }
};

struct ResAssetParam {
    u64 values;

    bool hasResParam(u32 type) const {
        return (values >> type & 1) == 1;
    }

    s32 getResParamIndex(u32 type) const {
        if (!hasResParam(type))
            return -1;

        return ::util::countOnBit(values, type) - 1;
    }
};

} // namespace xlink2