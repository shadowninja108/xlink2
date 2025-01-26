#include "util/common.h"
#include "util/file.h"
#include "util/sarc.h"

namespace util {

const std::vector<u8> Archive::cNullFile = {};

bool Archive::loadArchive(const std::string& path) {
    std::vector<u8> buffer{};
    if (!loadFileWithDecomp(path, buffer) || buffer.size() == 0)
        return false;

    auto header = reinterpret_cast<const ResArchiveHeader*>(buffer.data());

    if (header->magic != cSARCMagic || header->headerSize != sizeof(ResArchiveHeader) || header->bom != 0xfeff)
        return false;

    const uintptr_t dataPtr = reinterpret_cast<uintptr_t>(header) + header->dataOffset;
    auto sfat = reinterpret_cast<const ResFileAllocationTableHeader*>(header + 1);

    if (sfat->magic != cSFATMagic || sfat->headerSize != sizeof(ResFileAllocationTableHeader))
        return false;
    
    auto files = reinterpret_cast<const ResFileAllocationTableEntry*>(sfat + 1);
    auto sfnt = reinterpret_cast<const ResFileNameTableHeader*>(files + sfat->fileCount);

    if (sfnt->magic != cSFNTMagic || sfnt->headerSize != sizeof(ResFileNameTableHeader))
        return false;
    
    auto filenames = reinterpret_cast<const char*>(sfnt + 1);

    for (u32 i = 0; i < sfat->fileCount; ++i) {
        const std::string_view filename = filenames;
        const u32 hash = calcHash(filenames, sfat->hashMult);
        u32 low = 0;
        u32 high = sfat->fileCount - 1;
        while (low <= high) {
            const u32 middle = (low + high) / 2;
            auto file = files[middle];
            if (hash == file.filenameHash) {
                mFiles.emplace(filenames, std::vector<u8>(reinterpret_cast<const u8*>(dataPtr + file.dataStartOffset),
                                                                reinterpret_cast<const u8*>(dataPtr + file.dataEndOffset)));
                break;
            } else if (hash < file.filenameHash) {
                high = middle - 1;
            } else {
                low = middle + 1;
            }
        }
        filenames += align(filename.size() + 1, 0x4);
    }

    return mFiles.size() == sfat->fileCount;
}

const std::vector<std::string_view> Archive::getFilenames() const {
    std::vector<std::string_view> filenames{};
    for (const auto& [filename, data] : mFiles) {
        filenames.emplace_back(filename);
    }
    return filenames;
}

const std::vector<u8>& Archive::getFile(const std::string_view& path) const {
    for (const auto& [filename, data] : mFiles) {
        if (filename == path)
            return data;
    }
    return cNullFile;
}

} // namespace util