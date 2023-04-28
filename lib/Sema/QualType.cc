#include "Sema/QualType.h"

#include <sstream>

#include "Sema/Type.h"

using namespace scatha;
using namespace sema;

std::string QualType::makeName(ObjectType* base,
                               TypeQualifiers qualifiers,
                               size_t arraySize) {
    using enum TypeQualifiers;
    std::stringstream sstr;
    bool const isRef =
        test(qualifiers & (ExplicitReference | ImplicitReference));
    if (!test(qualifiers & Array)) {
        if (isRef) {
            sstr << "&";
        }
        if (test(qualifiers & Mutable)) {
            sstr << "mut ";
        }
        sstr << base->name();
    }
    else {

        if (isRef) {
            sstr << "&";
        }
        if (test(qualifiers & Mutable)) {
            sstr << "mut ";
        }
        sstr << "[" << base->name();
        if (!isRef && arraySize != QualType::DynamicArraySize) {
            sstr << ", " << arraySize;
        }
        sstr << "]";
    }
    return std::move(sstr).str();
}
