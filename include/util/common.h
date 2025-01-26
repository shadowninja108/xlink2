#pragma once

#include "util/types.h"

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
    return std::popcount(static_cast<T>(value & mask));
}

} // namespace util