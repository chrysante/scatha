#include "Sema/LifetimeMetadata.h"

using namespace scatha;
using namespace sema;

std::ostream& sema::operator<<(std::ostream& str, LifetimeOperation op) {
    using enum LifetimeOperation::Kind;
    switch (op.kind()) {
    case Trivial:
        str << "trivial";
    case Nontrivial:
        str << "nontrivial";
    case NontrivialInline:
        str << "nontrivial(inline)";
    case Deleted:
        str << "deleted";
    }
    return str;
}
