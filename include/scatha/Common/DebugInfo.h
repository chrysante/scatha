#ifndef SCATHA_COMMON_DEBUGINFO_H_
#define SCATHA_COMMON_DEBUGINFO_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Metadata.h>
#include <scatha/Common/SourceLocation.h>
#include <utl/hashmap.hpp>

namespace scatha::dbi {

/// List of source file paths to be associated with a target
class SourceFileList:
    public Metadata,
    public std::vector<std::filesystem::path> {
public:
    SourceFileList(vector elems): vector(std::move(elems)) {}

private:
    SourceFileList(SourceFileList const&) = default;

    std::unique_ptr<Metadata> doClone() const final;
    void doPrettyPrint(std::ostream& os) const final;
};

/// Source location to be associated with an instruction
class SourceLocationMD: public Metadata, public SourceLocation {
public:
    SourceLocationMD(SourceLocation sl): SourceLocation(sl) {}

private:
    SourceLocationMD(SourceLocationMD const&) = default;

    std::unique_ptr<Metadata> doClone() const final;
    void doPrettyPrint(std::ostream& os) const final;
};

///
struct DebugInfoMap {
    std::vector<std::filesystem::path> sourceFiles;
    utl::hashmap<size_t, SourceLocation> sourceLocationMap;

    std::string serialize() const;
};

} // namespace scatha::dbi

#endif // SCATHA_COMMON_DEBUGINFO_H_
