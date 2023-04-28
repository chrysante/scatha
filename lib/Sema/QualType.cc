#include "Sema/QualType.h"

#include <sstream>

#include "Sema/Type.h"

using namespace scatha;
using namespace sema;

std::string QualType::makeName(ObjectType* base, TypeQualifiers qualifiers) {
    std::stringstream sstr;
    using enum TypeQualifiers;
    if (test(qualifiers & (ImplicitReference | ExplicitReference))) {
        sstr << "&";
    }
    if (test(qualifiers & Mutable)) {
        sstr << "mut ";
    }
    sstr << base->name();
    return std::move(sstr).str();
}
