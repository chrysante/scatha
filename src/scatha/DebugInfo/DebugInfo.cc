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

} // namespace scatha

nlohmann::json DebugInfoMap::serialize() const {
    nlohmann::json j;
    j["files"] = sourceFiles;

    auto& labels = j["labels"];
    for (auto& [pos, label]: labelMap)
        labels.push_back({ { "pos", pos }, { "label", label } });

    auto& sourcemap = j["sourcemap"];
    for (auto& [pos, SL]: sourceLocationMap)
        sourcemap.push_back({ { "pos", pos }, { "loc", SL } });

    return j;
}

DebugInfoMap DebugInfoMap::deserialize(nlohmann::json const& j) {
    DebugInfoMap map;
    auto& labels = j.at("labels");
    for (auto& elem: labels)
        map.labelMap.insert({ elem.at("pos").get<size_t>(),
                              elem.at("label").get<std::string>() });
    auto& sourcemap = j.at("sourcemap");
    for (auto& elem: sourcemap)
        map.sourceLocationMap.insert({ elem.at("pos").get<size_t>(),
                                       elem.at("loc").get<SourceLocation>() });
    return map;
}
