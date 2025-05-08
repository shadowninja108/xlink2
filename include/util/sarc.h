#pragma once

#include "util/types.h"

#include <string>
#include <unordered_map>

// readonly sarc parser for LE sarcs

namespace util {

constexpr u32 makeMagic(const char (&str)[5]) {
    return static_cast<u32>(str[0] | (str[1] << 0x8) | (str[2] << 0x10) | (str[3] << 0x18));
}

const u32 cSARCMagic = makeMagic("SARC");
const u32 cSFNTMagic = makeMagic("SFNT");
const u32 cSFATMagic = makeMagic("SFAT");

inline u32 calcHash(std::string_view filename, u32 mult) {
    u32 hash = 0;
    for (const auto c : filename)
        hash = hash * mult + c;
    return hash;
}

struct ResArchiveHeader {
    u32 magic;
    u16 headerSize;
    u16 bom;
    u32 fileSize;
    u32 dataOffset;
    u16 version;
    char padding[2];
};
static_assert(sizeof(ResArchiveHeader) == 0x14);

struct ResFileAllocationTableHeader {
    u32 magic;
    u16 headerSize;
    u16 fileCount;
    u32 hashMult;
};
static_assert(sizeof(ResFileAllocationTableHeader) == 0xc);

struct ResFileAllocationTableEntry {
    u32 filenameHash;
    struct {
        u32 fileNameTableOffset : 0x18;
        u32 collisionCount      :  0x8;
    } fileAttributes;
    u32 dataStartOffset;
    u32 dataEndOffset;
};
static_assert(sizeof(ResFileAllocationTableEntry) == 0x10);

struct ResFileNameTableHeader {
    u32 magic;
    u16 headerSize;
    char padding[2];
};
static_assert(sizeof(ResFileNameTableHeader) == 0x8);

class Archive {
public:
    Archive() = default;
    ~Archive() = default;

    bool loadArchive(const std::string& path);

    const std::vector<std::string_view> getFilenames() const;
    const std::vector<u8>& getFile(const std::string_view& path) const;

    const static std::vector<u8> cNullFile;

private:
    std::unordered_map<std::string, std::vector<u8>> mFiles;
};

} // namespace util