#include "Model/SourceDebugInfo.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "Model/Disassembler.h"

using namespace sdb;

template <typename R>
static R findWithDefault(auto const& map, auto key, R def = {}) {
    auto itr = map.find(key);
    if (itr == map.end()) {
        return def;
    }
    return itr->second;
}

std::optional<SourceLocation> SourceLocationMap::toSourceLoc(
    size_t offset) const {
    return findWithDefault<std::optional<SourceLocation>>(offsetToSrcLoc,
                                                          offset);
}

std::span<uint32_t const> SourceLocationMap::toOffsets(
    SourceLocation SL) const {
    return findWithDefault<std::span<uint32_t const>>(srcLocToOffsets, SL);
}

std::span<uint32_t const> SourceLocationMap::toOffsets(size_t lineNum) const {
    return findWithDefault<std::span<uint32_t const>>(srcLineToOffsets,
                                                      lineNum);
}

namespace sdb {

void to_json(nlohmann::json& json, SourceLocation SL) {
    json = nlohmann::json{
        size_t(SL.fileIndex),
        size_t(SL.textIndex),
        size_t(SL.line),
        size_t(SL.column),
    };
}

void from_json(nlohmann::json const& json, SourceLocation& SL) {
    SL = {
        json.at(0).get<size_t>(),
        json.at(1).get<size_t>(),
        json.at(2).get<uint32_t>(),
        json.at(3).get<uint32_t>(),
    };
}

} // namespace sdb

SourceDebugInfo SourceDebugInfo::Load(std::filesystem::path path,
                                      Disassembly const& disasm) {
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

    auto& sourceMap = result._sourceMap;
    for (auto [index, jsonSL]: json["sourcemap"] | ranges::views::enumerate) {
        try {
            auto SL = jsonSL.get<SourceLocation>();
            size_t offset = disasm.indexToOffset(index);
            sourceMap.offsetToSrcLoc.insert({ offset, SL });
            sourceMap.srcLocToOffsets[SL].push_back(offset);
            sourceMap.srcLineToOffsets[size_t(SL.line)].push_back(offset);
        }
        catch (nlohmann::json::exception const& e) {
        }
    }
    for (auto& [SL, offsets]: sourceMap.srcLocToOffsets) {
        ranges::sort(offsets);
    }
    for (auto& [line, offsets]: sourceMap.srcLineToOffsets) {
        ranges::sort(offsets);
    }

    return result;
}
