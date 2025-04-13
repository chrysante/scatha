#ifndef SCATHA_COMMON_DEBUGMETADATA_H_
#define SCATHA_COMMON_DEBUGMETADATA_H_

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <scatha/Common/Metadata.h>
#include <scatha/Common/SourceLocation.h>
#include <utl/hashmap.hpp>

namespace scatha {

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

} // namespace scatha

#endif // SCATHA_COMMON_DEBUGMETADATA_H_
