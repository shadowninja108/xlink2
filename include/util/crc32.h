#pragma once

#include "util/types.h"

#include <array>
#include <string>

namespace util {

constexpr std::array<u32, 0x100> initializeCRC32Table();

u32 calcCRC32(const char* str);
u32 calcCRC32(const std::string_view str);

} // namespace util