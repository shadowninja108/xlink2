#pragma once

#include "resource.h"

#include <string>
#include <variant>

namespace banana {

struct Param {
    xlink2::ValueReferenceType type;
    std::variant<u32, std::string_view> value;
    s32 index;
};

struct ParamSet {
    std::vector<Param> params;
};

} // namespace banana