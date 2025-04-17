#ifndef SDB_MODEL_SOURCEDEBUGINFO_H_
#define SDB_MODEL_SOURCEDEBUGINFO_H_

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <scdis/Disassembly.h>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Model/SourceFile.h"

namespace scatha {
struct DebugInfoMap;
}

namespace sdb {

struct SourceLine {
    uint32_t file;
    uint32_t line;

    bool operator==(SourceLine const&) const = default;
};

struct SourceLocation {
    SourceLine line;
    uint32_t column;

    bool operator==(SourceLocation const&) const = default;
};

} // namespace sdb

template <>
struct std::hash<sdb::SourceLine> {
    size_t operator()(sdb::SourceLine SL) const {
        return utl::hash_combine(SL.file, SL.line);
    }
};

template <>
struct std::hash<sdb::SourceLocation> {
    size_t operator()(sdb::SourceLocation SL) const {
        return utl::hash_combine(SL.line, SL.column);
    }
};

namespace sdb {

class SourceDebugInfo;

/// Maps source locations to instruction pointer offsets and vice versa
class SourceLocationMap {
public:
    /// \Returns the source location of binary offset \p offset if possible
    std::optional<SourceLocation> toSourceLoc(
        scdis::InstructionPointerOffset ipo) const;

    /// \Returns the binary offsets associated with the source location \p
    /// sourceLoc
    std::span<scdis::InstructionPointerOffset const> toIpos(
        SourceLocation sourceLoc) const;

    /// \Returns the binary offsets associated with the line number
    /// \p sourceLine
    std::span<scdis::InstructionPointerOffset const> toIpos(
        SourceLine sourceLine) const;

private:
    friend class SourceDebugInfo;

    utl::hashmap<scdis::InstructionPointerOffset, SourceLocation> ipoToSrcLoc;
    utl::hashmap<SourceLocation,
                 utl::small_vector<scdis::InstructionPointerOffset>>
        srcLocToIpos;
    utl::hashmap<SourceLine, utl::small_vector<scdis::InstructionPointerOffset>>
        srcLineToIpos;
};

///
class SourceDebugInfo {
public:
    ///
    static SourceDebugInfo Make(scatha::DebugInfoMap const& map);

    ///
    std::span<SourceFile const> files() const { return _files; }

    /// \Returns `true` if no source debug information is available
    bool empty() const { return _files.empty(); }

    ///
    SourceLocationMap const& sourceMap() const { return _sourceMap; }

private:
    std::vector<SourceFile> _files;
    SourceLocationMap _sourceMap;
};

} // namespace sdb

#endif // SDB_MODEL_SOURCEDEBUGINFO_H_
