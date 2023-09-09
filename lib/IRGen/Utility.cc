#include "IRGen/Utility.h"

#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

sema::ArrayType const* irgen::ptrToArray(sema::ObjectType const* type) {
    auto* ptr = dyncast<sema::PointerType const*>(type);
    if (!ptr) {
        return nullptr;
    }
    return dyncast<sema::ArrayType const*>(ptr->base().get());
}

sema::QualType irgen::stripRefOrPtr(sema::QualType type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::PointerType const& ptr) {
            return ptr.base();
        },
        [](sema::ReferenceType const& ref) {
            return ref.base();
        },
        [&](sema::ObjectType const&) {
            return type;
        },
    }; // clang-format off
}