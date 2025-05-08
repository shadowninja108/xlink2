#pragma once

#include "util/types.h"
#include "util/error.h"

#include <array>
#include <bit>
#include <string>
#include <type_traits>

namespace util {

inline uintptr_t align(uintptr_t address, size_t alignment) {
    return (address + (alignment - 1)) & ~(alignment - 1);
}

// counts the number of set bits starting from the provided bit (going from most to least significant)
// includes the starting bit if set
template <typename T>
u32 countOnBit(T value, u32 bit) {
    static_assert(std::is_unsigned<T>(), "Can only bit count unsigned values");
    const T mask = ((1u << bit) - 1) | (1u << bit);
    return static_cast<u32>(std::popcount(static_cast<T>(value & mask)));
}

template <typename T, size_t N, std::enable_if_t<std::is_enum_v<T>>* = nullptr>
T matchEnum(std::string_view str, const std::array<std::string_view, N> values) {
    for (u32 i = 0;const auto& value : values) {
        if (str == value)
            return static_cast<T>(i);
        ++i;
    }

    throw InvalidDataError("Failed to match enum value!");
}

} // namespace util