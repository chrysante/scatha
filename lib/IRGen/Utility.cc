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
    return dyncast<sema::ArrayType const*>(getPtrOrRefBase(type));
}

bool irgen::isPtrOrRefToArray(sema::Type const* type) {
    return ptrOrRefToArrayImpl(type) != nullptr;
}

bool irgen::isPtrOrRefToDynArray(sema::Type const* type) {
    auto* AT = ptrOrRefToArrayImpl(type);
    return AT && AT->isDynamic();
}

std::optional<size_t> irgen::getStaticArraySize(sema::Type const* type) {
    auto* AT = ptrOrRefToArrayImpl(type);
    if (!AT && !(AT = dyncast<sema::ArrayType const*>(type))) {
        return std::nullopt;
    }
    if (AT->isDynamic()) {
        return std::nullopt;
    }
    return AT->count();
}

/// # THESE ARE OLD

ir::StructType const* irgen::makeArrayViewType(ir::Context& ctx) {
    return ctx.anonymousStruct({ ctx.ptrType(), ctx.intType(64) });
}
