#include "Sema/Analysis/Utility.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/DtorStack.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

/// # Destructos
/// We also implement DtorStack here to save a .cc file

static std::optional<DestructorCall> makeDTorCall(Object* obj) {
    auto* type = obj->type();
    auto* compType = dyncast<CompoundType const*>(type);
    if (!compType) {
        return std::nullopt;
    }
    using enum SpecialLifetimeFunction;
    auto* dtor = compType->specialLifetimeFunction(Destructor);
    if (!dtor) {
        return std::nullopt;
    }
    return DestructorCall{ obj, dtor };
}

void DTorStack::push(Object* obj) {
    if (auto dtorCall = makeDTorCall(obj)) {
        push(*dtorCall);
    }
}

void DTorStack::push(DestructorCall dtorCall) { dtorCalls.push(dtorCall); }

void sema::popTopLevelDtor(ast::Expression* expr, DTorStack& dtors) {
    if (!expr || !expr->isDecorated()) {
        return;
    }
    auto* compType = dyncast<CompoundType const*>(expr->type().get());
    if (!compType) {
        return;
    }
    using enum SpecialLifetimeFunction;
    if (compType->specialLifetimeFunction(Destructor)) {
        SC_ASSERT(expr->object() == dtors.top().object,
                  "We want to prolong the lifetime of the object defined by "
                  "expr, so that object better be on top of the stack");
        dtors.pop();
    }
}

/// # Special lifetime functions

namespace {

/// Wrapper around `std::array` that can be indexed by `SpecialLifetimeFunction`
/// enum values
struct SLFArray: std::array<Function*, EnumSize<SpecialLifetimeFunction>> {
    using Base = std::array<Function*, EnumSize<SpecialLifetimeFunction>>;
    auto& operator[](SpecialLifetimeFunction index) {
        return Base::operator[](static_cast<size_t>(index));
    }
    auto const& operator[](SpecialLifetimeFunction index) const {
        return Base::operator[](static_cast<size_t>(index));
    }
};

} // namespace

static FunctionSignature makeLifetimeSignature(SpecialLifetimeFunction key,
                                               ObjectType& type,
                                               SymbolTable& sym) {
    auto* self = sym.reference(QualType::Mut(&type));
    auto* rhs = sym.reference(QualType::Const(&type));
    auto* ret = sym.Void();
    using enum SpecialLifetimeFunction;
    switch (key) {
    case DefaultConstructor:
        return FunctionSignature({ self }, ret);
    case CopyConstructor:
        return FunctionSignature({ self, rhs }, ret);
    case MoveConstructor:
        return FunctionSignature({ self, rhs }, ret);
    case Destructor:
        return FunctionSignature({ self }, ret);
    }
}

static Function* generateSLF(SpecialLifetimeFunction key,
                             CompoundType& type,
                             SymbolTable& sym) {
    auto SMFKind = toSMF(key);
    Function* function = sym.withScopeCurrent(&type, [&] {
        return sym.declareFuncName(std::string(toString(SMFKind)));
    });
    SC_ASSERT(function, "Name can't be used by other symbol");
    bool result =
        sym.setFuncSig(function, makeLifetimeSignature(key, type, sym));
    SC_ASSERT(result, "Can't be defined because we checked in getDefinedSLFs");
    function->setKind(FunctionKind::Generated);
    function->setIsMember();
    function->setSLFKind(key);
    function->setSMFKind(SMFKind);
    type.addSpecialMemberFunction(SMFKind, function->overloadSet());
    return function;
}

static SLFArray getDefinedSLFs(SymbolTable& sym, StructType& type) {
    using enum SpecialMemberFunction;
    using enum SpecialLifetimeFunction;
    SLFArray result{};
    Type const* mutRef = sym.reference(QualType::Mut(&type));
    Type const* constRef = sym.reference(QualType::Const(&type));
    if (auto* constructor = type.specialMemberFunction(New)) {
        for (auto* F: *constructor) {
            switch (F->argumentCount()) {
            case 1: {
                if (F->argumentType(0) == mutRef) {
                    result[DefaultConstructor] = F;
                }
                break;
            }
            case 2: {
                if (F->argumentType(0) == mutRef &&
                    F->argumentType(1) == constRef)
                {
                    result[CopyConstructor] = F;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    if (auto* os = type.specialMemberFunction(Move)) {
        if (auto* move = os->find(std::array{ mutRef, mutRef })) {
            result[MoveConstructor] = move;
        }
    }
    if (auto* os = type.specialMemberFunction(Delete)) {
        if (auto* del = os->find(std::array{ mutRef })) {
            result[Destructor] = del;
        }
    }
    return result;
}

static bool computeDefaultConstructible(StructType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunction;
    using enum SpecialMemberFunction;
    /// If we have a default constructor we are clearly default constructible
    if (SLF[DefaultConstructor]) {
        return true;
    }
    auto* overloadSet = type.specialMemberFunction(New);
    /// If no constructors are defined or if the only defined constructor is the
    /// copy constructor, then the type is default constructible iff. all member
    /// variables are default constructible
    if (!overloadSet || (overloadSet->size() == 1 && SLF[CopyConstructor])) {
        return ranges::all_of(type.memberVariables(), [](auto* var) {
            return !var->type() || var->type()->isDefaultConstructible();
        });
    }
    /// Otherwise we are not default constructible
    return false;
}

static bool computeTrivialLifetime(StructType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunction;
    return !SLF[CopyConstructor] && !SLF[MoveConstructor] && !SLF[Destructor] &&
           ranges::all_of(type.memberVariables(), [](auto* var) {
               return !var->type() || var->type()->hasTrivialLifetime();
           });
}

static void declareSLFs(StructType& type, SymbolTable& sym) {
    using enum SpecialLifetimeFunction;
    auto SLF = getDefinedSLFs(sym, type);
    bool const isDefaultConstructible = computeDefaultConstructible(type, SLF);
    bool const hasTrivialLifetime = computeTrivialLifetime(type, SLF);
    type.setIsDefaultConstructible(isDefaultConstructible);
    type.setHasTrivialLifetime(hasTrivialLifetime);
    if (isDefaultConstructible && !SLF[DefaultConstructor]) {
        bool anyMemberHasDefCtor = ranges::any_of(type.memberVariables(),
                                                  [](auto* var) {
            auto* type = dyncast_or_null<StructType const*>(var->type());
            return type && type->specialLifetimeFunction(DefaultConstructor);
        });
        if (anyMemberHasDefCtor) {
            SLF[DefaultConstructor] =
                generateSLF(DefaultConstructor, type, sym);
        }
    }
    if (!hasTrivialLifetime) {
        if (!SLF[CopyConstructor]) {
            SLF[CopyConstructor] = generateSLF(CopyConstructor, type, sym);
        }
        if (!SLF[MoveConstructor]) {
            SLF[MoveConstructor] = generateSLF(MoveConstructor, type, sym);
        }
        if (!SLF[Destructor]) {
            SLF[Destructor] = generateSLF(Destructor, type, sym);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
}

static void declareSLFs(ArrayType& type, SymbolTable& sym) {
    auto* elemType = dyncast<CompoundType*>(type.elementType());
    if (!elemType) {
        return;
    }
    SLFArray SLF;
    for (auto key: EnumRange<SpecialLifetimeFunction>()) {
        if (elemType->specialLifetimeFunction(key)) {
            SLF[key] = generateSLF(key, type, sym);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
}

void sema::declareSpecialLifetimeFunctions(CompoundType& type,
                                           SymbolTable& sym) {
    visit(type, [&](auto& type) { declareSLFs(type, sym); });
}

/// # Other utils

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

StructType const* sema::nonTrivialLifetimeType(ObjectType const* type) {
    auto structType = dyncast<StructType const*>(type);
    if (!structType || structType->hasTrivialLifetime()) {
        return nullptr;
    }
    return structType;
}
