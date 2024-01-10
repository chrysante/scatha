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

sema::ArrayType const* irgen::getDynArrayType(sema::Type const* type) {
    auto* AT = dyncast<sema::ArrayType const*>(type);
    if (AT && AT->isDynamic()) {
        return AT;
    }
    return nullptr;
}

static sema::ArrayType const* ptrOrRefToArrayImpl(sema::Type const* type) {
    sema::Type const* base = getPtrOrRefBase(type);
    base = base ? base : type;
    return dyncast<sema::ArrayType const*>(base);
}

bool irgen::isFatPointer(sema::Type const* type) {
    auto* AT = ptrOrRefToArrayImpl(type);
    return AT && AT->isDynamic();
}

bool irgen::isFatPointer(ast::Expression const* expr) {
    return isFatPointer(expr->type().get());
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

ValueLocation irgen::commonLocation(ValueLocation a, ValueLocation b) {
    return a == b ? a : ValueLocation::Register;
}
