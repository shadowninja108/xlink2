#include "util/file.h"
#include "util/sarc.h"
#include "system.h"

#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_FILEPATH 0x1000

static std::string parseInput(int argc, char** argv, s32 index) {
    static std::string null_string;

    if (argc < 2 + index) {
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

    const std::string opt = parseInput(argc, argv, 0);

    if (opt.empty() || opt == "-h" || opt == "--help") {
        constexpr std::string_view helpMessage = \
        "XLink2 Resource File Conversion Tool\n"
        "-----------------------------------------------------------------------------------------\n"
        "Usage:\n"
        "Converting XLNK to YAML (final option is optional, include if decompression is desired)\n"
        "  r--export [path_to_xlink_file] [output_yaml_path] [path_to_zsdic_pack]\n"
        "Converting YAML to XLNK (final option is optional, include if compression is desired)\n"
        "  --import [path_to_yaml] [output_xlink_path] [path_to_zsdic_pack]";
        std::cout << helpMessage;
    } else if (opt == "--export" || opt == "-e") {
        const std::string filepath = parseInput(argc, argv, 1);
        const std::string outputPath = parseInput(argc, argv, 2);
        const std::string dictPath = parseInput(argc, argv, 3);

        if (dictPath.empty()) {
            std::vector<u8> buffer{};
            util::loadFile(filepath, buffer);

            banana::System sys;
            if (!sys.initialize(buffer.data(), buffer.size())) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }
            const std::string text = sys.dumpYAML();
            util::writeFile(outputPath, {reinterpret_cast<const u8*>(text.data()), text.size()}, false);
        } else {
            util::Archive archive;
            if (!archive.loadArchive(dictPath)) {
                std::cerr << "failed to load dictionaries!\n";
                return 1;
            }

            auto filenames = archive.getFilenames();
            std::vector<std::vector<u8>> dicts(filenames.size());
            for (u32 i = 0; const auto& filename : filenames) {
                dicts[i] = archive.getFile(filename);
                ++i;
            }

            std::vector<u8> buffer{};
            if (!util::loadFileWithDecomp(filepath, buffer, dicts)) {
                std::cerr << "failed to load file!\n";
                return 1;
            }

            banana::System sys;
            if (!sys.initialize(buffer.data(), buffer.size())) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }

            auto text = sys.dumpYAML();
            util::writeFile(outputPath, {reinterpret_cast<const u8*>(text.data()), text.size()}, false);
        }
    } else if (opt == "--import" || opt == "-i") {
        const std::string filepath = parseInput(argc, argv, 1);
        const std::string outputPath = parseInput(argc, argv, 2);
        const std::string dictPath = parseInput(argc, argv, 3);

        if (dictPath.empty()) {
            std::vector<u8> buffer{};
            util::loadFile(filepath, buffer);

            banana::System sys;
            if (!sys.loadYAML({reinterpret_cast<char*>(buffer.data()), buffer.size()})) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }

            const auto data = sys.serialize();
            util::writeFile(outputPath, {data.data(), data.size()}, false);
        } else {
            util::Archive archive;
            if (!archive.loadArchive(dictPath)) {
                std::cerr << "failed to load dictionaries!\n";
                return 1;
            }

            const auto dictData = archive.getFile("zs.zsdic");

            std::vector<u8> buffer{};
            util::loadFile(filepath, buffer);

            banana::System sys;
            if (!sys.loadYAML({reinterpret_cast<char*>(buffer.data()), buffer.size()})) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }

            const auto data = sys.serialize();
            const std::span<const u8> dict = {dictData.data(), dictData.size()};
            util::writeFile(outputPath, {data.data(), data.size()}, true, dict);
        }
    } else if (opt == "--roundtrip") {
        const std::string filepath = parseInput(argc, argv, 1);
        const std::string outputPath = parseInput(argc, argv, 2);
        const std::string dictPath = parseInput(argc, argv, 3);


        if (dictPath.empty()) {
            std::vector<u8> buffer{};
            util::loadFile(filepath, buffer);

            banana::System sys;
            if (!sys.initialize(buffer.data(), buffer.size())) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }
            const auto data = sys.serialize();
            util::writeFile(outputPath, {reinterpret_cast<const u8*>(data.data()), data.size()}, false);
        } else {
            util::Archive archive;
            if (!archive.loadArchive(dictPath)) {
                std::cerr << "failed to load dictionaries!\n";
                return 1;
            }

            auto filenames = archive.getFilenames();
            std::vector<std::vector<u8>> dicts(filenames.size());
            for (u32 i = 0; const auto& filename : filenames) {
                dicts[i] = archive.getFile(filename);
                ++i;
            }

            std::vector<u8> buffer{};
            if (!util::loadFileWithDecomp(filepath, buffer, dicts)) {
                std::cerr << "failed to load file!\n";
                return 1;
            }

            banana::System sys;
            if (!sys.initialize(buffer.data(), buffer.size())) {
                std::cerr << "Failed to parse file!\n";
                return 1;
            }

            const auto data = sys.serialize();
            util::writeFile(outputPath, {reinterpret_cast<const u8*>(data.data()), data.size()}, false);
        }
    } else {
        std::cout << "Unknown option! Please use --help for usage";
    }

    return 0;
}