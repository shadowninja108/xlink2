#pragma once

#include "util/types.h"

#include <span>
#include <string>
#include <vector>

namespace util {

bool loadFile(const std::string& path, std::vector<u8>& buffer);
bool loadFileWithDecomp(const std::string& path, std::vector<u8>& buffer, const std::vector<std::vector<u8>>& dict = {});
void writeFile(const std::string& path, const std::span<const u8>& data, bool compress, const std::span<const u8>& dict = {});

} // namespace util