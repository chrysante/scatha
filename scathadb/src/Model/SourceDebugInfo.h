#ifndef SDB_MODEL_SOURCEDEBUGINFO_H_
#define SDB_MODEL_SOURCEDEBUGINFO_H_

#include <filesystem>
#include <span>
#include <vector>

#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Model/SourceFile.h"

namespace sdb {

struct SourceLocation {
    size_t fileIndex : 12;
    size_t textIndex : 50;
    uint32_t line;
    uint32_t column;

    bool operator==(SourceLocation const&) const = default;
};

} // namespace sdb

template <>
struct std::hash<sdb::SourceLocation> {
    size_t operator()(sdb::SourceLocation SL) const {
        return utl::hash_combine(SL.fileIndex,
                                 SL.textIndex,
                                 SL.line,
                                 SL.column);
    }
};

namespace sdb {

class Disassembly;
class SourceDebugInfo;

///
class SourceLocationMap {
public:
    /// \Returns the source location of binary offset \p offset if possible
    std::optional<SourceLocation> toSourceLoc(size_t offset) const;

    /// \Returns the binary offsets associated with the source location \p
    /// sourceLoc
    std::span<uint32_t const> toOffsets(SourceLocation sourceLoc) const;

    /// \Returns the binary offsets associated with the line number \p lineNum
    std::span<uint32_t const> toOffsets(size_t lineNum) const;

private:
    friend class SourceDebugInfo;

    utl::hashmap<size_t, SourceLocation> offsetToSrcLoc;
    utl::hashmap<SourceLocation, utl::small_vector<uint32_t>> srcLocToOffsets;
    utl::hashmap<size_t, utl::small_vector<uint32_t>> srcLineToOffsets;
};

///
class SourceDebugInfo {
public:
    ///
    static SourceDebugInfo Load(std::filesystem::path path,
                                Disassembly const& disasm);

    ///
    std::span<SourceFile const> files() const { return _files; }

    /// \Returns `true` if no source debug information is available
    bool empty() const { return _files.empty(); }

    ///
    SourceLocationMap const& sourceMap() const { return _sourceMap; }

private:
    SourceLocationMap _sourceMap;
    std::vector<SourceFile> _files;
};

} // namespace sdb

#endif // SDB_MODEL_SOURCEDEBUGINFO_H_
