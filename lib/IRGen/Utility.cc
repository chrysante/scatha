#include "IRGen/Utility.h"

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
        [](sema::Type const& type) {
            return nullptr;
        }
    }; // clang-format on
}

sema::ArrayType const* ptrOrRefToArrayImpl(sema::Type const* type) {
    return dyncast_or_null<sema::ArrayType const*>(getPtrOrRefBase(type));
}

bool irgen::isPtrOrRefToArray(sema::Type const* type) {
    return ptrOrRefToArrayImpl(type) != nullptr;
}

bool irgen::isPtrOrRefToDynArray(sema::Type const* type) {
    auto* AT = ptrOrRefToArrayImpl(type);
    return AT && AT->isDynamic();
}

/// # THESE ARE OLD

sema::ArrayType const* irgen::ptrToArray(sema::ObjectType const* type) {
    if (auto* ptr = dyncast<sema::PointerType const*>(type)) {
        return dyncast<sema::ArrayType const*>(ptr->base().get());
    }
    return nullptr;
}

sema::Type const* irgen::stripRefOrPtr(sema::Type const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::PointerType const& ptr) {
            return ptr.base().get();
        },
        [](sema::ReferenceType const& ref) {
            return ref.base().get();
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

bool irgen::isArrayPtrOrArrayRef(sema::Type const* type) {
    return isa<sema::ArrayType>(stripRefOrPtr(type)) && (isa<sema::PointerType>(type) || isa<sema::ReferenceType>(type));
}

ir::StructType const* irgen::makeArrayViewType(ir::Context& ctx) {
    std::array<ir::Type const*, 2> members = { ctx.ptrType(), ctx.intType(64) };
    return ctx.anonymousStruct(members);
}
