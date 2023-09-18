#include "IRGen/Utility.h"

#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

sema::ArrayType const* irgen::ptrToArray(sema::ObjectType const* type) {
    if (auto* ptr = dyncast<sema::PointerType const*>(type)) {
        return dyncast<sema::ArrayType const*>(ptr->base().get());
    }
    return nullptr;
}

sema::ArrayType const* irgen::ptrOrRefToArray(sema::ObjectType const* type) {
    if (auto* ptr = ptrToArray(stripReference(type).get())) {
        return ptr;
    }
    if (auto* ref = dyncast<sema::ReferenceType const*>(type)) {
        return dyncast<sema::ArrayType const*>(ref->base().get());
    }
    return nullptr;
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

bool irgen::isArrayAndDynamic(sema::ObjectType const* type) {
    auto* arrayType = dyncast<sema::ArrayType const*>(type);
    return arrayType && arrayType->isDynamic();
}

bool irgen::isArrayPtrOrArrayRef(sema::ObjectType const* type) {
    return isa<sema::ArrayType>(stripRefOrPtr(type).get()) && (isa<sema::PointerType>(type) || isa<sema::ReferenceType>(type));
}

ir::StructType const* irgen::makeArrayViewType(ir::Context& ctx) {
    std::array<ir::Type const*, 2> members = { ctx.ptrType(), ctx.intType(64) };
    return ctx.anonymousStruct(members);
}
