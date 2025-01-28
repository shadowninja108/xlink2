#include "util/file.h"

#include <zstd.h>

#include <cstring>
#include <filesystem>
#include <fstream>

namespace util {

bool loadFile(const std::string& path, std::vector<u8>& buffer) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    buffer.resize(file.tellg());
    file.seekg(0);

    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

    file.close();

    return true;
}

bool loadFileWithDecomp(const std::string& path, std::vector<u8>& buffer, const std::vector<std::vector<u8>>& dicts) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        return false;

    std::vector<u8> srcBuffer{};

    srcBuffer.resize(file.tellg());
    file.seekg(0);

    file.read(reinterpret_cast<char*>(srcBuffer.data()), srcBuffer.size());

    if (srcBuffer.size() < 4 || *reinterpret_cast<const u32*>(srcBuffer.data()) != ZSTD_MAGICNUMBER) {
        buffer.resize(srcBuffer.size());
        std::memcpy(buffer.data(), srcBuffer.data(), srcBuffer.size());
        return true;
    }

    const size_t decompressedSize = ZSTD_getFrameContentSize(srcBuffer.data(), srcBuffer.size());

    if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN || decompressedSize == ZSTD_CONTENTSIZE_ERROR)
        return false;

    buffer.resize(decompressedSize);

    if (dicts.size() == 0) {
        const size_t res = ZSTD_decompress(buffer.data(), buffer.size(), srcBuffer.data(), srcBuffer.size());

        return !ZSTD_isError(res);
    } else {
        const u32 expectedDictID = ZSTD_getDictID_fromFrame(srcBuffer.data(), srcBuffer.size());
        if (expectedDictID == 0) {
            const size_t res = ZSTD_decompress(buffer.data(), buffer.size(), srcBuffer.data(), srcBuffer.size());
            
            return !ZSTD_isError(res);
        }
        u32 i = 0;
        for (const auto& dict : dicts) {
            const u32 dictID = ZSTD_getDictID_fromDict(dict.data(), dict.size());
            if (expectedDictID == dictID) {
                break;
            }
            ++i;
        }
        if (i >= dicts.size())
            return false;
        
        ZSTD_DDict* const ddict = ZSTD_createDDict(dicts[i].data(), dicts[i].size());
        ZSTD_DCtx* const dctx = ZSTD_createDCtx();

        const size_t res = ZSTD_decompress_usingDDict(dctx, buffer.data(), buffer.size(), srcBuffer.data(), srcBuffer.size(), ddict);

        ZSTD_freeDCtx(dctx);
        ZSTD_freeDDict(ddict);

        return !ZSTD_isError(res);
    }
}

void writeFile(const std::string& path, const std::span<const u8>& data, bool compress, const std::span<const u8>& dict) {
    std::vector<u8> fileData{};
    if (compress && dict.size() > 0) {
        const size_t compressionBufferSize = ZSTD_compressBound(data.size());
        fileData.resize(compressionBufferSize);

        ZSTD_CCtx* const cctx = ZSTD_createCCtx();
        ZSTD_compress_usingDict(cctx, fileData.data(), fileData.size(), data.data(), data.size(), dict.data(), dict.size(), 22);
        ZSTD_freeCCtx(cctx);
    } else {
        fileData.resize(data.size());
        std::memcpy(fileData.data(), data.data(), data.size());
    }

    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    file.close();
}

} // namespace util