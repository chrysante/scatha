#include "Model/SourceDebugInfo.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scatha/DebugInfo/DebugInfo.h>

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
    scdis::InstructionPointerOffset ipo) const {
    return findWithDefault<std::optional<SourceLocation>>(ipoToSrcLoc, ipo);
}

std::span<scdis::InstructionPointerOffset const> SourceLocationMap::toIpos(
    SourceLocation sourceLoc) const {
    return findWithDefault<std::span<scdis::InstructionPointerOffset const>>(
        srcLocToIpos, sourceLoc);
}

std::span<scdis::InstructionPointerOffset const> SourceLocationMap::toIpos(
    size_t sourceLine) const {
    return findWithDefault<std::span<scdis::InstructionPointerOffset const>>(
        srcLineToIpos, sourceLine);
}

static sdb::SourceLocation convertSourceLoc(scatha::SourceLocation sl) {
    return { sl.fileIndex, (size_t)sl.index, (uint32_t)sl.line,
             (uint32_t)sl.column };
}

SourceDebugInfo SourceDebugInfo::Make(scatha::DebugInfoMap const& map) {
    SourceDebugInfo result;
    for (auto& file: map.sourceFiles)
        result._files.push_back(SourceFile::Load(file));
    auto& sourceMap = result._sourceMap;
    for (auto& [offset, sl]: map.sourceLocationMap) {
        scdis::InstructionPointerOffset ipo(offset);
        auto sourceLoc = convertSourceLoc(sl);
        sourceMap.ipoToSrcLoc.insert({ ipo, sourceLoc });
        sourceMap.srcLocToIpos[sourceLoc].push_back(ipo);
        sourceMap.srcLineToIpos[sourceLoc.line].push_back(ipo);
    }
    for (auto& [SL, ipos]: sourceMap.srcLocToIpos)
        ranges::sort(ipos);
    for (auto& [line, ipos]: sourceMap.srcLineToIpos)
        ranges::sort(ipos);
    return result;
}
