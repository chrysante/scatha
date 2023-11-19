#include "Model/SourceDebugInfo.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace sdb;

SourceDebugInfo SourceDebugInfo::Load(std::filesystem::path path) {
    auto file = std::fstream(path, std::ios::in);
    if (!file) {
        return {};
    }
    SourceDebugInfo result;
    auto json = nlohmann::json::parse(file);
    for (auto file: json["files"]) {
        result._files.push_back(
            SourceFile::Load(path.parent_path() / file.get<std::string>()));
    }
    return result;
}
