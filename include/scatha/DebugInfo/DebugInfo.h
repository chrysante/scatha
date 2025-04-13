#ifndef SCATHA_DEBUGINFO_DEBUGINFO_H_
#define SCATHA_DEBUGINFO_DEBUGINFO_H_

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>
#include <utl/hashtable.hpp>

#include <scatha/Common/SourceLocation.h>

namespace scatha {

/// Structure containing all debug metadata emitted by the compiler
struct DebugInfoMap {
    /// List of absolute source file paths. This array can be indexed by the
    /// file indices of source locations.
    std::vector<std::filesystem::path> sourceFiles;

    /// Maps binary offsets to label names.
    utl::hashmap<size_t, std::string> labelMap;

    /// Maps binary offsets to source locations.
    utl::hashmap<size_t, SourceLocation> sourceLocationMap;

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
