#include "Common/DebugMetadata.h"

using namespace scatha;

std::unique_ptr<Metadata> SourceFileList::doClone() const {
    return std::unique_ptr<SourceFileList>(new SourceFileList(*this));
}

void SourceFileList::doPrettyPrint(std::ostream& os) const {
    for (bool first = true; auto& path: *this) {
        if (!first) os << ", ";
        first = false;
        os << path;
    }
}

std::unique_ptr<Metadata> InstructionDebugMetadata::doClone() const {
    return std::unique_ptr<InstructionDebugMetadata>(
        new InstructionDebugMetadata(*this));
}

void InstructionDebugMetadata::doPrettyPrint(std::ostream& os) const {
    os << sourceLocation();
}
