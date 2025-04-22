#include "DebugInfo/DebugInfo.h"

#include <nlohmann/json.hpp>

using namespace scatha;

namespace scatha {

static void to_json(nlohmann::json& j, SourceLocation const& loc) {
    j = {
        (i64)loc.fileIndex,
        (i64)loc.index,
        (i64)loc.line,
        (i64)loc.column,
    };
}

static void from_json(nlohmann::json const& j, SourceLocation& loc) {
    loc.fileIndex = j.at(0).get<size_t>();
    loc.index = j.at(1).get<i64>();
    loc.line = j.at(2).get<i32>();
    loc.column = j.at(3).get<i32>();
}

static void to_json(nlohmann::json& j, DebugLabel const& label) {
    j = { { "type", label.type }, { "name", label.name } };
}

static void from_json(nlohmann::json const& j, DebugLabel& label) {
    label.type = j.at("type").get<DebugLabel::Type>();
    label.name = j.at("name").get<std::string>();
}

static void to_json(nlohmann::json& j, IpoRange const& ipoRange) {
    j = { { "begin", ipoRange.begin }, { "end", ipoRange.end } };
}

static void from_json(nlohmann::json const& j, IpoRange& ipoRange) {
    ipoRange.begin = j.at("begin").get<size_t>();
    ipoRange.end = j.at("end").get<size_t>();
}

} // namespace scatha

nlohmann::json DebugInfoMap::serialize() const {
    nlohmann::json j;
    j["files"] = sourceFiles;
    for (auto& labels = j["labels"]; auto& [pos, label]: labelMap)
        labels.push_back({ { "pos", pos }, { "label", label } });
    for (auto& sourcemap = j["sourcemap"]; auto& [pos, SL]: sourceLocationMap)
        sourcemap.push_back({ { "pos", pos }, { "loc", SL } });
    for (auto& fnIpoMap = j["functionipomap"];
         auto& [name, ipoRange]: functionIpoMap)
        fnIpoMap.push_back({ { "function", name }, { "range", ipoRange } });
    return j;
}

DebugInfoMap DebugInfoMap::deserialize(nlohmann::json const& j) {
    DebugInfoMap map;
    map.sourceFiles = j.at("files");
    for (auto& labels = j.at("labels"); auto& elem: labels)
        map.labelMap.insert({ elem.at("pos").get<size_t>(),
                              elem.at("label").get<DebugLabel>() });
    for (auto& sourcemap = j.at("sourcemap"); auto& elem: sourcemap)
        map.sourceLocationMap.insert({ elem.at("pos").get<size_t>(),
                                       elem.at("loc").get<SourceLocation>() });
    for (auto& fnIpoMap = j.at("functionipomap"); auto& elem: fnIpoMap)
        map.functionIpoMap.insert({ elem.at("function").get<std::string>(),
                                    elem.at("range").get<IpoRange>() });
    return map;
}
