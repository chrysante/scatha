#include "Sema/LifetimeMetadata.h"

using namespace scatha;
using namespace sema;

std::ostream& sema::operator<<(std::ostream& str, LifetimeOperation op) {
    using enum LifetimeOperation::Kind;
    switch (op.kind()) {
    case Trivial:
        return str << "trivial";
    case Nontrivial:
        return str << "nontrivial";
    case NontrivialInline:
        return str << "nontrivial(inline)";
    case Deleted:
        return str << "deleted";
    }
}
