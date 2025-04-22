#include "Model/SourceDebugInfo.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scatha/DebugInfo/DebugInfo.h>

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
    scdis::InstructionPointerOffset ipo) const {
    return findWithDefault<std::optional<SourceLocation>>(ipoToSrcLoc, ipo);
}

std::span<scdis::InstructionPointerOffset const> SourceLocationMap::toIpos(
    SourceLocation sourceLoc) const {
    return findWithDefault<std::span<scdis::InstructionPointerOffset const>>(
        srcLocToIpos, sourceLoc);
}

std::span<scdis::InstructionPointerOffset const> SourceLocationMap::toIpos(
    SourceLine sourceLine) const {
    return findWithDefault<std::span<scdis::InstructionPointerOffset const>>(
        srcLineToIpos, sourceLine);
}

static sdb::SourceLocation convertSourceLoc(scatha::SourceLocation sl) {
    return { { (uint32_t)sl.fileIndex, (uint32_t)sl.line },
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
    for (auto& [functionName, ipoRange]: map.functionIpoMap)
        result._functionInfoMap.push_back(
            { functionName, scdis::InstructionPointerOffset(ipoRange.begin),
              scdis::InstructionPointerOffset(ipoRange.end) });
    ranges::sort(result._functionInfoMap, ranges::less{},
                 &FunctionDebugInfo::begin);
    return result;
}

FunctionDebugInfo const* SourceDebugInfo::findFunction(
    scdis::InstructionPointerOffset ipo) const {
    auto first = _functionInfoMap.begin();
    auto last = _functionInfoMap.end();
    while (first < last) {
        auto mid = first + (last - first) / 2;
        if (ipo < mid->begin)
            last = mid;
        else if (ipo >= mid->end)
            first = mid;
        else
            return std::to_address(mid);
    }
    return nullptr;
}
