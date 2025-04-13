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

std::unique_ptr<Metadata> SourceLocationMD::doClone() const {
    return std::unique_ptr<SourceLocationMD>(new SourceLocationMD(*this));
}

void SourceLocationMD::doPrettyPrint(std::ostream& os) const { os << *this; }
