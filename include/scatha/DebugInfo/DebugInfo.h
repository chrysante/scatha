#ifndef SCATHA_DEBUGINFO_DEBUGINFO_H_
#define SCATHA_DEBUGINFO_DEBUGINFO_H_

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <utl/hashtable.hpp>

#include <scatha/Common/SourceLocation.h>

namespace scatha {

struct DebugLabel {
    enum Type { Function, BasicBlock, StringData, RawData };
    Type type;
    std::string name;
};

struct IpoRange {
    /// The offset of the first instruction in this range.
    size_t begin;

    /// The past-the-end offset of this range.
    size_t end;
};

/// Structure containing all debug metadata emitted by the compiler
struct DebugInfoMap {
    /// List of absolute source file paths. This array can be indexed by the
    /// file indices of source locations.
    std::vector<std::filesystem::path> sourceFiles;

    /// Maps binary offsets to labels.
    utl::hashmap<size_t, DebugLabel> labelMap;

    /// Maps binary offsets to source locations.
    utl::hashmap<size_t, SourceLocation> sourceLocationMap;

    /// Maps (mangled) function names to ranges of instruction pointer offsets
    utl::hashmap<std::string, IpoRange> functionIpoMap;

    /// \Returns true if all members are empty
    bool empty() const {
        return sourceFiles.empty() && labelMap.empty() &&
               sourceLocationMap.empty();
    }

    ///
    nlohmann::json serialize() const;

    ///
    static DebugInfoMap deserialize(nlohmann::json const& json);
};

} // namespace scatha

#endif // SCATHA_DEBUGINFO_DEBUGINFO_H_
