#include "Common/DebugInfo.h"

#include <nlohmann/json.hpp>
#include <range/v3/view.hpp>

using namespace scatha;
using namespace dbi;

static nlohmann::json serialize(SourceFileList const* list) {
    nlohmann::json result;
    if (!list) {
        return result;
    }
    for (auto [index, path]: *list | ranges::views::enumerate) {
        result[index] = path;
    }
    return result;
}

static nlohmann::json toJSON(SourceLocation loc) {
    return { size_t(loc.fileIndex),
             size_t(loc.index),
             size_t(loc.line),
             size_t(loc.column) };
}

static nlohmann::json serialize(
    std::span<SourceLocation const* const> sourceLocations) {
    nlohmann::json result;
    for (auto [index, SL]: sourceLocations | ranges::views::enumerate) {
        auto& elem = result[index];
        if (SL) {
            elem = toJSON(*SL);
        }
    }
    return result;
}

std::string dbi::serialize(
    SourceFileList const* sourceFiles,
    std::span<SourceLocation const* const> sourceLocations) {
    nlohmann::json data = {
        { "files", ::serialize(sourceFiles) },
        { "sourcemap", ::serialize(sourceLocations) },
    };
    return data.dump();
}
