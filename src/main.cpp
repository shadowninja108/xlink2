#include "util/file.h"
#include "util/sarc.h"
#include "system.h"

#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_FILEPATH 0x1000

const std::string parseInput(int argc, char** argv, s32 index) {
    static constexpr std::string null_string{""};

    if (argc < 2) {
        return null_string;
    }

    size_t size = strnlen(argv[1 + index], MAX_FILEPATH);
    std::string value{argv[1 + index], argv[1 + index] + size};
    return value;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "Starting\n";

    std::string filepath = parseInput(argc, argv, 0);
    std::string dictPath = parseInput(argc, argv, 1);

    std::cout << "Loading files\n";

    util::Archive archive;
    if (!archive.loadArchive(dictPath)) {
        std::cout << "failed to load dictionaries!\n";
        return 1;
    }

    auto filenames = archive.getFilenames();

    std::vector<std::vector<u8>> dicts(filenames.size());
    for (u32 i = 0; const auto& filename : filenames) {
        dicts[i] = archive.getFile(filename);
        ++i;
    }

    std::vector<u8> buffer;
    if (!util::loadFileWithDecomp(filepath, buffer, dicts)) {
        std::cout << "failed to load file!\n";
        return 1;
    }

    std::cout << "Parsing file\n";

    banana::System system;
    system.initialize(buffer.data(), buffer.size());

    // system.getPDT().printParams();

    // system.printUser("Player");

    // auto data = system.serialize();

    // util::writeFile("out.bin", data, false);
    // auto dict = archive.getFile("zs.zsdic");
    // util::writeFile("out.bin.zs", data, true, {dict.data(), dict.size()});

    std::cout << "Exporting as YAML\n";

    auto yaml = system.dumpYAML();
    util::writeFile("out.yaml", {reinterpret_cast<const u8*>(yaml.data()), yaml.size()}, false);

    std::cout << "Done!\n";

    return 0;
}