#include "util/crc32.h"

namespace util {

constexpr std::array<u32, 0x100> initializeCRC32Table() {
    std::array<u32, 0x100> table;

    for (u32 i = 0; i < table.size(); ++i) {
        u32 value = i;
        for (u32 j = 0; j < 8; ++j) {
            value = ((value & 1) == 0) ? (value >> 1) : (0xedb88320 ^ (value >> 1));
        }
        table[i] = value;
    }

    return table;
}

static constexpr std::array<u32, 0x100> sCRC32Table = initializeCRC32Table();

u32 calcCRC32(const char* str) {
    u32 hash = 0xffffffff;

    const u8* ptr = reinterpret_cast<const u8*>(str);
    while (*ptr)
        hash = sCRC32Table[*ptr++ ^ (hash & 0xff)] ^ (hash >> 8);
    
    return ~hash;
}

u32 calcCRC32(const std::string_view str) {
    u32 hash = 0xffffffff;

    const u8* ptr = reinterpret_cast<const u8*>(str.data());
    for (size_t i = 0; i < str.size(); ++i) {
        hash = sCRC32Table[*ptr++ ^ (hash & 0xff)] ^ (hash >> 8);
    }

    return ~hash;
}

} // namespace util