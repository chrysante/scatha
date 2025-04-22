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

/// Debug metadata to be associated with an instruction
class InstructionDebugMetadata: public Metadata {
public:
    InstructionDebugMetadata(std::string functionName, SourceLocation sl):
        _fnName(functionName), _sl(sl) {}

    std::string const& functionName() const { return _fnName; }

    SourceLocation const& sourceLocation() const { return _sl; }

private:
    InstructionDebugMetadata(InstructionDebugMetadata const&) = default;

    std::unique_ptr<Metadata> doClone() const final;
    void doPrettyPrint(std::ostream& os) const final;

    std::string _fnName;
    SourceLocation _sl;
};

} // namespace scatha

#endif // SCATHA_COMMON_DEBUGMETADATA_H_
