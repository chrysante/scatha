#include "Sema/Analysis/Utility.h"

#include <array>
#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/CleanupStack.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using enum ValueCategory;
using enum ConversionKind;

/// # Cleanups
/// We also implement `CleanupStack` here to save a .cc file

/// Creates a cleanup operation from \p object
/// This may return nullopt because the entity could be of reference type or
/// invalid type. Even if the operation is deleted or trivial an engaged
/// optional is returned
static std::optional<CleanupOperation> makeCleanup(Object* obj) {
    if (!obj) {
        return std::nullopt;
    }
    auto* type = dyncast<ObjectType const*>(obj->type());
    if (!type) {
        return std::nullopt;
    }
    return CleanupOperation{ obj, type->lifetimeMetadata().destructor() };
}

bool CleanupStack::push(Object* obj, AnalysisContext& ctx) {
    auto cleanup = makeCleanup(obj);
    if (!cleanup || cleanup->operation.isTrivial()) {
        /// We return `true` because no cleanup is required. No error occured
        /// here
        return true;
    }
    if (cleanup->operation.isDeleted()) {
        ctx.issue<BadCleanup>(obj->astNode(), obj);
        return false;
    }
    operations.push(*cleanup);
    return true;
}

bool CleanupStack::push(ast::Expression* expr, AnalysisContext& ctx) {
    if (expr->isValue()) {
        return push(expr->object(), ctx);
    }
    return true;
}

void CleanupStack::pop(Object* obj) {
    auto cleanup = makeCleanup(obj);
    if (!cleanup || cleanup->operation.isTrivial()) {
        return;
    }
    SC_ASSERT(*cleanup == top(),
              "We want to prolong the lifetime of the object defined by "
              "expr, so that object better be on top of the stack");
    operations.pop();
}

void CleanupStack::pop(ast::Expression* expr) {
    if (!expr->isDecorated()) {
        return;
    }
    if (expr->isValue()) {
        pop(expr->object());
    }
}

void CleanupStack::erase(Object* obj) {
    auto cleanup = makeCleanup(obj);
    if (!cleanup || cleanup->operation.isTrivial()) {
        return;
    }
    auto& cont = operations.container();
    auto itr = ranges::find(cont, cleanup);
    SC_ASSERT(itr != cont.end(), "cleanup is not in this stack");
    cont.erase(itr);
}

void CleanupStack::erase(ast::Expression* expr) {
    if (expr->isValue()) {
        erase(expr->object());
    }
}

void sema::print(CleanupStack const& stack, std::ostream& str) {
    for (auto& call: stack) {
        // clang-format off
        SC_MATCH (*call.object) {
            [&](Variable const& var) {
                str << var.name();
            },
            [&](Temporary const& tmp) {
                str << "tmp[" << tmp.id() << "]";
            },
            [&](Property const&) { SC_UNREACHABLE(); },
        }; // clang-format on
        str << std::endl;
    }
}

void sema::print(CleanupStack const& stack) { print(stack, std::cout); }

/// # Other utils

Function* sema::findBySignature(std::span<Function* const> functions,
                                std::span<Type const* const> types) {
    std::span<Function const* const> cf(functions.data(), functions.size());
    return const_cast<Function*>(findBySignature(cf, types));
}

Function const* sema::findBySignature(
    std::span<Function const* const> functions,
    std::span<Type const* const> types) {
    auto itr = ranges::find_if(functions, [&](auto* function) {
        return ranges::equal(function->argumentTypes(), types);
    });
    return itr != functions.end() ? *itr : nullptr;
}

QualType sema::getQualType(Type const* type, Mutability mut) {
    if (auto* ref = dyncast<ReferenceType const*>(type)) {
        return ref->base();
    }
    return { cast<ObjectType const*>(type), mut };
}

ValueCategory sema::refToLValue(Type const* type) {
    if (isa<ReferenceType>(type)) {
        return LValue;
    }
    return RValue;
}

ast::Statement* sema::parentStatement(ast::ASTNode* node) {
    while (true) {
        if (!node) {
            return nullptr;
        }
        if (auto* stmt = dyncast<ast::Statement*>(node)) {
            return stmt;
        }
        node = node->parent();
    }
}

CompoundType const* sema::nonTrivialLifetimeType(ObjectType const* type) {
    auto cType = dyncast<CompoundType const*>(type);
    if (!cType || cType->hasTrivialLifetime()) {
        return nullptr;
    }
    return cType;
}

AccessControl sema::determineAccessControlByContext(Scope const& scope) {
    // clang-format off
    return SC_MATCH (scope) {
        [](StructType const& type) {
            return type.accessControl();
        },
        [](FileScope const&) {
            return AccessControl::Internal;
        },
        [](GlobalScope const&) {
            return AccessControl::Internal;
        },
        [](Scope const&) -> AccessControl {
            SC_UNREACHABLE();
        }
    }; // clang-format on
}

AccessControl sema::determineAccessControl(Scope const& scope,
                                           ast::Declaration const& decl) {
    if (auto specified = decl.accessControl()) {
        return *specified;
    }
    return determineAccessControlByContext(scope);
}

ArrayType const* sema::dynArrayTypeCast(Type const* type) {
    auto* AT = dyncast<ArrayType const*>(type);
    if (AT && AT->isDynamic()) {
        return AT;
    }
    return nullptr;
}

bool sema::isDynArray(Type const& type) { return !!dynArrayTypeCast(&type); }

/// We define `isUserDefined` as a function object to pass it to higher order
/// ranges functions
struct {
    bool operator()(Function const* f) const { return f->isNative(); }
    bool operator()(LifetimeOperation op) const {
        auto* f = op.function();
        return f && (*this)(f);
    }
} static constexpr isUserDefined{};

bool sema::isAggregate(Type const* t) {
    auto* type = dyncast<StructType const*>(t);
    if (!type) {
        return false;
    }
    if (ranges::any_of(type->lifetimeMetadata().operations(), isUserDefined)) {
        return false;
    }
    if (ranges::any_of(type->constructors(), isUserDefined)) {
        return false;
    }
    return ranges::all_of(type->memberVariables(), [&](Variable const* var) {
        return var->accessControl() <= type->accessControl();
    });
}

bool sema::isNewMoveDelete(sema::Function const& F) {
    return ranges::contains(std::array{ "new", "move", "delete" }, F.name());
}
