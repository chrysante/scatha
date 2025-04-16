#include "Model/SourceDebugInfo.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

using namespace sdb;

std::string sdb::toString(SourceLocation const& SL) {
    std::stringstream sstr;
    sstr << "L:" << SL.line << ", C:" << SL.column << ", F:" << SL.fileIndex;
    return std::move(sstr).str();
}

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

SourceDebugInfo SourceDebugInfo::Load(std::filesystem::path path) {
    auto file = std::fstream(path, std::ios::in);
    if (!file) return {};
    SourceDebugInfo result;
    auto const json = nlohmann::json::parse(file);
    if (auto itr = json.find("files"); itr != json.end()) {
        for (auto file: *itr)
            result._files.push_back(
                SourceFile::Load(path.parent_path() / file.get<std::string>()));
    }
    if (auto itr = json.find("labels"); itr != json.end()) {
        auto& labelMap = result._labelMap;
        for (auto value: *itr) {
            auto pos = value.find("pos");
            auto label = value.find("label");
            if (pos != value.end() && label != value.end())
                labelMap.insert(
                    { pos->get<size_t>(), label->get<std::string>() });
        }
    }
    if (auto itr = json.find("sourcemap"); itr != json.end()) {
        auto& sourceMap = result._sourceMap;
        for (auto const& value: *itr) {
            auto pos = value.find("pos");
            auto loc = value.find("loc");
            if (pos != value.end() && loc != value.end()) {
                auto SL = loc->get<SourceLocation>();
                uint32_t offset = pos->get<uint32_t>();
                sourceMap.offsetToSrcLoc.insert({ offset, SL });
                sourceMap.srcLocToOffsets[SL].push_back(offset);
                sourceMap.srcLineToOffsets[size_t(SL.line)].push_back(offset);
            }
        }
        for (auto& [SL, offsets]: sourceMap.srcLocToOffsets)
            ranges::sort(offsets);
        for (auto& [line, offsets]: sourceMap.srcLineToOffsets)
            ranges::sort(offsets);
    }
    return result;
}

std::optional<std::string> SourceDebugInfo::labelName(size_t binOffset) const {
    auto itr = _labelMap.find(binOffset);
    if (itr != _labelMap.end()) return itr->second;
    return std::nullopt;
}
