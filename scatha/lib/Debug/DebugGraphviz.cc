#include "Debug/DebugGraphviz.h"

#include <stdexcept>

#include <utl/strcat.hpp>

using namespace scatha;
using namespace debug;

std::pair<std::filesystem::path, std::fstream> debug::newDebugFile() {
    static int counter = 0;
    return newDebugFile(utl::strcat("tmp", counter++));
}

std::pair<std::filesystem::path, std::fstream> debug::newDebugFile(
    std::string name) {
    std::filesystem::path dir = "debug";
    std::filesystem::create_directory(dir);
    std::filesystem::path filepath = dir / name;
    std::fstream file(filepath, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error(
            utl::strcat("Failed to open file: ", filepath));
    }
    return { filepath, std::move(file) };
}

void debug::createGraphAndOpen(std::filesystem::path gvPath) {
    auto svgPath = gvPath;
    svgPath += ".svg";
    std::system(utl::strcat("dot -Tsvg ", gvPath, " -o ", svgPath).c_str());
    std::system(utl::strcat("open ", svgPath).c_str());
}
