#include "IRGen/Utility.h"

#include "AST/AST.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

sema::ObjectType const* irgen::getPtrOrRefBase(sema::Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::ReferenceType const& type) {
            return type.base().get();
        },
        [](sema::PointerType const& type) {
            return type.base().get();
        },
        [](sema::Type const&) {
            return nullptr;
        }
    }; // clang-format on
}

bool irgen::isDynArray(sema::Type const* type) {
    auto* arr = dyncast<sema::ArrayType const*>(type);
    return arr && arr->isDynamic();
}

bool irgen::isDynArrayPointer(sema::Type const* type) {
    auto* ptr = dyncast<sema::PointerType const*>(type);
    return ptr && isDynArray(ptr->base().get());
}

bool irgen::isDynArrayReference(sema::Type const* type) {
    auto* ref = dyncast<sema::ReferenceType const*>(type);
    return ref && isDynArray(ref->base().get());
}

bool irgen::isDynPointer(sema::Type const* type) {
    auto* ptr = dyncast<sema::PointerType const*>(type);
    return ptr && ptr->base().isDyn();
}

bool irgen::isDynReference(sema::Type const* type) {
    auto* ref = dyncast<sema::ReferenceType const*>(type);
    return ref && ref->base().isDyn();
}

sema::ObjectType const* irgen::stripPtr(sema::ObjectType const* type) {
    if (auto* ptrType = dyncast<sema::PointerType const*>(type)) {
        return ptrType->base().get();
    }
    return type;
}

std::optional<size_t> irgen::getStaticArraySize(sema::ObjectType const* type) {
    auto* AT = dyncast<sema::ArrayType const*>(stripPtr(type));
    if (!AT || AT->isDynamic()) {
        return std::nullopt;
    }
    return AT->count();
}

ir::StructType const* irgen::makeArrayPtrType(ir::Context& ctx) {
    return ctx.anonymousStruct({ ctx.ptrType(), ctx.intType(64) });
}

ir::StructType const* irgen::makeDynPtrType(ir::Context& ctx) {
    return ctx.anonymousStruct({ ctx.ptrType(), ctx.ptrType() });
}
