from pathlib import Path
import sys
import zlib

if __name__ == "__main__":
    elink_users = []
    slink_users = []
    text = ""
    if (len(sys.argv) < 2):
        text = Path(input("User list filepath: ")).read_text()
    else:
        text = Path(sys.argv[1]).read_text()
    current = elink_users
    for line in text.split("\n"):
        if line == "SLink:":
            current = slink_users
        elif line == "ELink:":
            current = elink_users
        else:
            if line:
                current.append(line.strip())
    
    with open("include/usernames.inc", "w", encoding="utf-8") as f:
        f.write(
"""#pragma once

#include "util/types.h"

#include <string>
#include <unordered_map>

namespace banana {

const static inline std::unordered_map<u32, std::string_view> sELinkUserNames = {
"""
        )
        for user in elink_users:
            f.write(f"    {{{zlib.crc32(user.encode("utf-8")):#010x}, \"{user}\"}},\n")
        f.write(
"""};

const static inline std::unordered_map<u32, std::string_view> sSLinkUserNames = {
"""
        )
        for user in slink_users:
            f.write(f"    {{{zlib.crc32(user.encode("utf-8")):#010x}, \"{user}\"}},\n")
        f.write("};\n\n} // namespace banana")