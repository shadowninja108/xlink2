#pragma once

#include "util/types.h"

#include <string>
#include <vector>

namespace banana {
    
struct ArrangeGroupParam {
    std::string_view groupName;
    s8 limitType;
    s8 limitThreshold;
    u8 unk;
};

struct ArrangeGroupParams {
    std::vector<ArrangeGroupParam> groups;
};

} // namespace banana