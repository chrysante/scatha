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

static std::optional<CleanupOperation> makeCleanup(Object* obj) {
    using enum SpecialLifetimeFunctionDepr;
    auto* type = dyncast<ObjectType const*>(obj->type());
    if (!type) {
        return std::nullopt;
    }
    auto* dtor = type->specialLifetimeFunction(Destructor);
    if (!dtor) {
        return std::nullopt;
    }
    return CleanupOperation{ obj, LifetimeOperation(dtor) };
}

void CleanupStack::push(Object* obj) {
    if (auto dtorCall = makeCleanup(obj)) {
        push(*dtorCall);
    }
}

void CleanupStack::push(CleanupOperation cleanup) { operations.push(cleanup); }

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

void sema::popTopLevelDtor(ast::Expression* expr, CleanupStack& dtors) {
    if (!expr || !expr->isDecorated()) {
        return;
    }
    auto* type = expr->type().get();
    using enum SpecialLifetimeFunctionDepr;
    if (type && type->specialLifetimeFunction(Destructor)) {
        SC_ASSERT(expr->object() == dtors.top().object,
                  "We want to prolong the lifetime of the object defined by "
                  "expr, so that object better be on top of the stack");
        dtors.pop();
    }
}

/// # Special lifetime functions

namespace {

/// Wrapper around `std::array` that can be indexed by
/// `SpecialLifetimeFunctionDepr` enum values
struct SLFArray: std::array<Function*, EnumSize<SpecialLifetimeFunctionDepr>> {
    using Base = std::array<Function*, EnumSize<SpecialLifetimeFunctionDepr>>;
    auto& operator[](SpecialLifetimeFunctionDepr index) {
        return Base::operator[](static_cast<size_t>(index));
    }
    auto const& operator[](SpecialLifetimeFunctionDepr index) const {
        return Base::operator[](static_cast<size_t>(index));
    }
};

} // namespace

static FunctionType const* makeLifetimeSignature(
    SpecialLifetimeFunctionDepr key, ObjectType& type, SymbolTable& sym) {
    auto* self = sym.reference(QualType::Mut(&type));
    auto* rhs = sym.reference(QualType::Const(&type));
    auto* ret = sym.Void();
    using enum SpecialLifetimeFunctionDepr;
    switch (key) {
    case DefaultConstructor:
        return sym.functionType({ self }, ret);
    case CopyConstructor:
        return sym.functionType({ self, rhs }, ret);
    case MoveConstructor:
        return sym.functionType({ self, rhs }, ret);
    case Destructor:
        return sym.functionType({ self }, ret);
    }
    SC_UNREACHABLE();
}

static Function* generateSLF(SpecialLifetimeFunctionDepr key,
                             ObjectType& type,
                             SymbolTable& sym) {
    auto SMFKind = toSMF(key);
    Function* function = sym.withScopeCurrent(&type, [&] {
        return sym.declareFunction(std::string(toString(SMFKind)),
                                   makeLifetimeSignature(key, type, sym),
                                   type.accessControl());
    });
    SC_ASSERT(function, "Name can't be used by other symbol");
    function->setKind(FunctionKind::Generated);
    function->setSMFMetadata({ SMFKind, key });
    type.addSpecialMemberFunction(SMFKind, function);
    return function;
}

static SLFArray getDefinedSLFs(SymbolTable& sym, StructType& type) {
    using enum SpecialMemberFunctionDepr;
    using enum SpecialLifetimeFunctionDepr;
    SLFArray result{};
    Type const* mutRef = sym.reference(QualType::Mut(&type));
    Type const* constRef = sym.reference(QualType::Const(&type));
    auto ctors = type.specialMemberFunctions(New);
    for (auto* F: ctors) {
        switch (F->argumentCount()) {
        case 1: {
            if (F->argumentType(0) == mutRef) {
                result[DefaultConstructor] = F;
            }
            break;
        }
        case 2: {
            if (F->argumentType(0) == mutRef && F->argumentType(1) == constRef)
            {
                result[CopyConstructor] = F;
            }
            break;
        }
        default:
            break;
        }
    }

    auto moveCtors = type.specialMemberFunctions(Move);
    if (auto* move = findBySignature(moveCtors, std::array{ mutRef, mutRef })) {
        result[MoveConstructor] = move;
    }

    auto dtors = type.specialMemberFunctions(Delete);
    if (auto* del = findBySignature(dtors, std::array{ mutRef })) {
        result[Destructor] = del;
    }

    return result;
}

static bool computeDefaultConstructible(StructType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunctionDepr;
    using enum SpecialMemberFunctionDepr;
    /// If we have a default constructor we are clearly default constructible
    if (SLF[DefaultConstructor]) {
        return true;
    }
    /// Otherwise we are only default constructible if no special member
    /// functions are user defined
    return type.specialMemberFunctions(New).empty() &&
           type.specialMemberFunctions(Move).empty() &&
           type.specialMemberFunctions(Delete).empty();
}

static bool computeTrivialLifetime(StructType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunctionDepr;
    return !SLF[CopyConstructor] && !SLF[MoveConstructor] && !SLF[Destructor] &&
           ranges::all_of(type.memberVariables(), [](auto* var) {
        return !var->type() || var->type()->hasTrivialLifetime();
    });
}

static bool allMembersHave(StructType& type, SpecialLifetimeFunctionDepr fn) {
    SC_ASSERT(fn != SpecialLifetimeFunctionDepr::DefaultConstructor,
              "This function is only for copy/move constructor and destructor");
    return ranges::all_of(type.members(), [&](auto* type) {
        if (!type) {
            return true;
        }
        if (type->hasTrivialLifetime()) {
            return true;
        }
        auto* objType = cast<ObjectType const*>(type);
        return objType->specialLifetimeFunction(fn) != nullptr;
    });
}

static void declareSLFs(ObjectType&, SymbolTable&) { SC_UNREACHABLE(); }

static void declareSLFs(StructType& type, SymbolTable& sym) {
    using enum SpecialMemberFunctionDepr;
    using enum SpecialLifetimeFunctionDepr;
    auto SLF = getDefinedSLFs(sym, type);
    bool const isDefaultConstructible = computeDefaultConstructible(type, SLF);
    bool const hasTrivialLifetime = computeTrivialLifetime(type, SLF);
    type.setIsDefaultConstructible(isDefaultConstructible);
    type.setHasTrivialLifetime(hasTrivialLifetime);
    if (isDefaultConstructible && !SLF[DefaultConstructor]) {
        /// We generate the default constructor if it is necessary and possible
        bool anyMemberHasUserDefinedDefCtor =
            ranges::any_of(type.members(), [](auto* type) {
            auto* objType = cast<ObjectType const*>(type);
            return objType &&
                   objType->specialLifetimeFunction(DefaultConstructor);
        });
        bool allMembersAreDefaultConstructible =
            ranges::all_of(type.members(), [&](auto* type) {
            auto* objType = cast<ObjectType const*>(type);
            if (!objType) {
                return true;
            }
            if (objType->hasTrivialLifetime() &&
                objType->specialMemberFunctions(New).empty())
            {
                return true;
            }
            return objType->specialLifetimeFunction(DefaultConstructor) !=
                   nullptr;
        });
        if (anyMemberHasUserDefinedDefCtor && allMembersAreDefaultConstructible)
        {
            SLF[DefaultConstructor] =
                generateSLF(DefaultConstructor, type, sym);
        }
    }
    /// We generate SLFs if we have nontrivial lifetime and no SLFs are user
    /// defined
    if (!hasTrivialLifetime && !SLF[CopyConstructor] && !SLF[MoveConstructor] &&
        !SLF[Destructor])
    {
        if (allMembersHave(type, CopyConstructor)) {
            SLF[CopyConstructor] = generateSLF(CopyConstructor, type, sym);
        }
        if (allMembersHave(type, MoveConstructor)) {
            SLF[MoveConstructor] = generateSLF(MoveConstructor, type, sym);
        }
        if (allMembersHave(type, Destructor)) {
            SLF[Destructor] = generateSLF(Destructor, type, sym);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
    for (auto [index, F]: SLF | enumerate) {
        if (F) {
            auto md = F->getSMFMetadata();
            SC_EXPECT(md);
            F->setSMFMetadata(
                { md->kind(), (SpecialLifetimeFunctionDepr)index });
        }
    }
}

static void declareSLFs(ArrayType& type, SymbolTable& sym) {
    auto* elemType = type.elementType();
    SLFArray SLF{};
    for (auto key: EnumRange<SpecialLifetimeFunctionDepr>()) {
        if (elemType->specialLifetimeFunction(key)) {
            SLF[key] = generateSLF(key, type, sym);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
}

static void declareSLFs(UniquePtrType& type, SymbolTable& sym) {
    using enum SpecialLifetimeFunctionDepr;
    SLFArray SLF{};
    SLF[DefaultConstructor] = generateSLF(DefaultConstructor, type, sym);
    SLF[CopyConstructor] = nullptr;
    SLF[MoveConstructor] = generateSLF(MoveConstructor, type, sym);
    SLF[Destructor] = generateSLF(Destructor, type, sym);
    type.setSpecialLifetimeFunctions(SLF);
}

void sema::declareSpecialLifetimeFunctions(ObjectType& type, SymbolTable& sym) {
    visit(type, [&](auto& type) { declareSLFs(type, sym); });
}

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

sema::ArrayType const* sema::dynArrayTypeCast(sema::Type const* type) {
    auto* AT = dyncast<sema::ArrayType const*>(type);
    if (AT && AT->isDynamic()) {
        return AT;
    }
    return nullptr;
}

bool sema::isDynArray(sema::Type const& type) {
    return !!dynArrayTypeCast(&type);
}

ast::Expression* sema::insertConstruction(ast::Expression* expr,
                                          CleanupStack& dtors,
                                          AnalysisContext& ctx) {
    auto* type = expr->type().get();
    if (type->hasTrivialLifetime()) {
        auto* constr =
            ast::insertNode<ast::TrivCopyConstructExpr>(expr,
                                                        expr->sourceRange(),
                                                        type);
        return analyzeValueExpr(constr, dtors, ctx);
    }
    else {
        SC_UNIMPLEMENTED();
    }
}
