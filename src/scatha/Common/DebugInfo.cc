#include "Common/DebugInfo.h"

#include <nlohmann/json.hpp>
#include <range/v3/view.hpp>

using namespace scatha;
using namespace dbi;

std::unique_ptr<Metadata> SourceFileList::doClone() const {
    return std::unique_ptr<SourceFileList>(new SourceFileList(*this));
}

void SourceFileList::doPrettyPrint(std::ostream& os) const {
    for (bool first = true; auto& path: *this) {
        if (!first) os << ", ";
        first = false;
        os << path;
    }
}

std::unique_ptr<Metadata> SourceLocationMD::doClone() const {
    return std::unique_ptr<SourceLocationMD>(new SourceLocationMD(*this));
}

void SourceLocationMD::doPrettyPrint(std::ostream& os) const { os << *this; }

static nlohmann::json serialize(std::span<std::filesystem::path const> list) {
    nlohmann::json result;
    for (auto& path: list)
        result.push_back(path);
    return result;
}

static nlohmann::json serialize(
    std::span<std::pair<size_t, std::string> const> labels) {
    nlohmann::json result;
    for (auto& [pos, label]: labels)
        result.push_back({ { "pos", pos }, { "label", label } });
    return result;
}

namespace scatha {

static void to_json(nlohmann::json& j, SourceLocation const& loc) {
    j = { size_t(loc.fileIndex), size_t(loc.index), size_t(loc.line),
          size_t(loc.column) };
}

} // namespace scatha

static nlohmann::json serialize(
    std::span<std::pair<size_t, SourceLocation> const> sourceLocations) {
    nlohmann::json result;
    for (auto& [pos, SL]: sourceLocations)
        result.push_back({ { "pos", pos }, { "loc", SL } });
    return result;
}

std::string DebugInfoMap::serialize() const {
    nlohmann::json data;
    data["files"] = ::serialize(sourceFiles);
    data["labels"] = ::serialize(labelMap.values());
    data["sourcemap"] = ::serialize(sourceLocationMap.values());
    return data.dump();
}
