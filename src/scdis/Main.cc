#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <scbinutil/Utils.h>
#include <scdis/Disassembler.h>
#include <scdis/Print.h>
#include <utl/vector.hpp>

namespace {

struct Options {
    std::filesystem::path inputPath;
    bool useColor = false;
};

} // namespace

static int disasmMain(Options const& options);

int main(int argc, char* argv[]) {
    CLI::App app("Scatha Disassembler", "scdis");
    Options options;
    app.add_option("file", options.inputPath)->check(CLI::ExistingFile);
    app.add_flag("-c,--color", options.useColor);
    try {
        app.parse(argc, argv);
        return disasmMain(options);
    }
    catch (CLI::ParseError const& e) {
        return app.exit(e);
    }
    catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

static utl::vector<uint8_t> readFile(std::filesystem::path const& path) {
    std::fstream file(path, std::ios::in | std::ios::binary);
    if (!file) throw std::runtime_error("File does not exist");
    file.seekg(0, std::ios::end);
    ssize_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    utl::vector<uint8_t> data((size_t)size, utl::no_init);
    file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) throw std::runtime_error("Failed to read file");
    return data;
}

static scatha::DebugInfoMap readDebugInfo(std::filesystem::path inputPath) {
    inputPath += ".scdsym";
    std::fstream file(inputPath, std::ios::in);
    if (!file) return {};
    auto j = nlohmann::json::parse(file);
    return scatha::DebugInfoMap::deserialize(j);
}

static int disasmMain(Options const& options) {
    auto data = readFile(options.inputPath);
    auto debugInfo = readDebugInfo(options.inputPath);
    auto binary = scbinutil::seekBinary(data);
    auto disasm = scdis::disassemble(binary, debugInfo);
    scdis::print(disasm, std::cout, options.useColor);
    return 0;
}
