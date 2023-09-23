#include "Sema/Analysis/Instantiation.h"

#include <array>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/StructDependencyGraph.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/QualType.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct InstContext {
    InstContext(sema::AnalysisContext& ctx):
        ctx(ctx), sym(ctx.symbolTable()), iss(ctx.issueHandler()) {}

    std::vector<StructType const*> instantiateTypes(
        StructDependencyGraph& typeDependencies);

    void instantiateFunctions(std::span<ast::FunctionDefinition*> functions);

    void instantiateStructureType(SDGNode&);

    void instantiateVariable(SDGNode&);

    void instantiateFunction(ast::FunctionDefinition&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition&) const;

    Type const* analyzeParameter(ast::ParameterDeclaration&,
                                 size_t index) const;

    void generateSLFs(StructType& type);

    Function* generateSLF(SpecialLifetimeFunction key, StructType& type) const;

    FunctionSignature makeLifetimeSignature(
        StructType& type, SpecialLifetimeFunction function) const;

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
};

} // namespace

utl::vector<StructType const*> sema::instantiateEntities(
    AnalysisContext& ctx,
    StructDependencyGraph& typeDependencies,
    std::span<ast::FunctionDefinition*> functions) {
    InstContext instCtx(ctx);
    auto structs = instCtx.instantiateTypes(typeDependencies);
    for (auto* def: functions) {
        instCtx.instantiateFunction(*def);
    }
    /// `structs` is topologically sorted, so each invocation of
    /// `generateSLFs()` can assume that the types of all
    /// data members already have been analyzed for lifetime triviality
    for (auto* type: structs) {
        instCtx.generateSLFs(const_cast<StructType&>(*type));
    }
    return structs;
}

static bool isUserDefined(Type const* type) {
    // clang-format off
    return SC_MATCH(*type) {
        [](StructType const&) {
            return true;
        },
        [](ArrayType const& arrayType) {
            return isUserDefined(arrayType.elementType());
        },
        [](Type const&) {
            return false;
        }
    }; // clang-format on
}

std::vector<StructType const*> InstContext::instantiateTypes(
    StructDependencyGraph& dependencyGraph) {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    auto dataMembers = dependencyGraph | ranges::views::filter([](auto& node) {
                           return isa<Variable>(node.entity);
                       });
    for (auto& node: dataMembers) {
        auto& var = cast<ast::VariableDeclaration&>(*node.astNode);
        auto* type = sym.withScopeCurrent(node.entity->parent(), [&] {
            return analyzeTypeExpr(var.typeExpr(), ctx);
        });
        if (type && isUserDefined(type)) {
            node.dependencies.push_back(
                utl::narrow_cast<u16>(dependencyGraph.index(type)));
        }
    }
    /// Check for cycles
    auto indices = ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
                   ranges::to<utl::small_vector<u16>>;
    auto const cycle =
        utl::find_cycle(indices.begin(),
                        indices.end(),
                        [&](size_t index) -> auto const& {
                            return dependencyGraph[index].dependencies;
                        });
    if (!cycle.empty()) {
        auto entities = cycle | ranges::views::transform(
                                    [&](size_t index) -> Entity const* {
                                        return dependencyGraph[index].entity;
                                    });
        ctx.issue<StructDefCycle>(entities | ranges::to<std::vector>);
        return {};
    }
    /// Sort dependencies...
    auto dependencyTraversalOrder =
        ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
        ranges::to<utl::small_vector<u16>>;
    utl::topsort(dependencyTraversalOrder.begin(),
                 dependencyTraversalOrder.end(),
                 [&](size_t index) -> auto const& {
                     return dependencyGraph[index].dependencies;
                 });
    std::vector<StructType const*> sortedStructTypes;
    /// Instantiate all types and member variables.
    for (size_t const index: dependencyTraversalOrder) {
        auto& node = dependencyGraph[index];
        // clang-format off
        visit(*node.entity, utl::overload{
            [&](Variable const&) { instantiateVariable(node); },
            [&](StructType const& type) {
                instantiateStructureType(node);
                sortedStructTypes.push_back(&type);
            },
            [&](Entity const&) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
    return sortedStructTypes;
}

void InstContext::instantiateFunctions(
    std::span<ast::FunctionDefinition*> functions) {}

static std::optional<SpecialMemberFunction> toSMF(
    ast::FunctionDefinition const& funcDef) {
    for (uint8_t i = 0; i < EnumSize<SpecialMemberFunction>; ++i) {
        SpecialMemberFunction SMF{ i };
        if (toString(SMF) == funcDef.name()) {
            return SMF;
        }
    }
    return std::nullopt;
}

void InstContext::instantiateStructureType(SDGNode& node) {
    ast::StructDefinition& structDef =
        cast<ast::StructDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize = 0;
    size_t objectAlign = 0;
    auto& structType = cast<StructType&>(*structDef.entity());
    /// Here we collect all the variables and special member function of the
    /// struct
    for (auto [varIndex, varDecl]: structDef.body()->statements() |
                                       Filter<ast::VariableDeclaration> |
                                       ranges::views::enumerate)
    {
        if (!varDecl->isDecorated()) {
            continue;
        }
        auto& var = *varDecl->variable();
        structType.addMemberVariable(&var);
        if (!varDecl->type()) {
            continue;
        }
        if (isa<ReferenceType>(varDecl->type())) {
            ctx.issue<BadVarDecl>(varDecl,
                                  BadVarDecl::RefInStruct,
                                  varDecl->type(),
                                  varDecl->initExpr());
            continue;
        }
        auto* varType = varDecl->type();
        objectAlign = std::max(objectAlign, varType->align());
        SC_ASSERT(varType->size() % varType->align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, varType->align());
        size_t const currentOffset = objectSize;
        varDecl->setOffset(currentOffset);
        varDecl->setIndex(varIndex);
        var.setOffset(currentOffset);
        var.setIndex(varIndex);
        objectSize += varType->size();
    }
    if (objectAlign > 0) {
        objectSize = utl::round_up(objectSize, objectAlign);
    }
    structType.setSize(objectSize);
    structType.setAlign(objectAlign);
}

void InstContext::instantiateVariable(SDGNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto* type = analyzeTypeExpr(varDecl.typeExpr(), ctx);
    /// Here we set the TypeID of the variable in the symbol table.
    auto* variable = cast<Variable*>(node.entity);
    variable->setType(type);
    varDecl.decorateVarDecl(variable);
}

void InstContext::instantiateFunction(ast::FunctionDefinition& def) {
    auto* F = def.function();
    sym.makeScopeCurrent(F->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto result = sym.setFuncSig(F, analyzeSignature(def));
    if (!result) {
        return;
    }
    auto SMF = toSMF(def);
    if (!SMF) {
        return;
    }
    auto* type = dyncast<StructType*>(F->parent());
    type->addSpecialMemberFunction(*SMF, F->overloadSet());
    if (!type) {
        ctx.issue<BadSMF>(&def, BadSMF::NotInStruct, *SMF, type);
        return;
    }
    F->setSMFKind(*SMF);
    if (def.returnTypeExpr()) {
        ctx.issue<BadSMF>(&def, BadSMF::HasReturnType, *SMF, type);
        return;
    }
    F->setDeducedReturnType(sym.Void());
    Type const* mutRef = sym.reference(QualType::Mut(type));
    if (F->argumentCount() == 0) {
        ctx.issue<BadSMF>(&def, BadSMF::NoParams, *SMF, type);
        return;
    }
    if (F->argumentType(0) != mutRef) {
        ctx.issue<BadSMF>(&def, BadSMF::BadFirstParam, *SMF, type);
        return;
    }
    using enum SpecialMemberFunction;
    switch (F->SMFKind()) {
    case New:
        break;
    case Move:
        if (F->argumentCount() != 2 || F->argumentType(1) != mutRef) {
            ctx.issue<BadSMF>(&def, BadSMF::MoveSignature, *SMF, type);
            return;
        }
        break;
    case Delete:
        if (F->argumentCount() != 1) {
            ctx.issue<BadSMF>(&def, BadSMF::DeleteSignature, *SMF, type);
            return;
        }
        break;
    }
}

FunctionSignature InstContext::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    utl::small_vector<Type const*> argumentTypes;
    for (auto [index, param]: decl.parameters() | ranges::views::enumerate) {
        argumentTypes.push_back(analyzeParameter(*param, index));
    }
    /// If the return type is not specified it will be deduced during function
    /// analysis
    auto* returnType = decl.returnTypeExpr() ?
                           analyzeTypeExpr(decl.returnTypeExpr(), ctx) :
                           nullptr;
    return FunctionSignature(std::move(argumentTypes), returnType);
}

Type const* InstContext::analyzeParameter(ast::ParameterDeclaration& param,
                                          size_t index) const {
    auto* thisParam = dyncast<ast::ThisParameter const*>(&param);
    if (!thisParam) {
        return analyzeTypeExpr(param.typeExpr(), ctx);
    }
    auto* structure = param.findAncestor<ast::StructDefinition>();
    if (!structure) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisInFreeFunction);
        return nullptr;
    }
    if (index != 0) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisPosition);
        return nullptr;
    }
    auto* structType = cast<StructType const*>(structure->entity());
    if (thisParam->isReference()) {
        return sym.reference(QualType(structType, thisParam->mutability()));
    }
    return structType;
}

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
            return var->type()->isDefaultConstructible();
        });
    }
    /// Otherwise we are not default constructible
    return false;
}

static bool computeTrivialLifetime(StructType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunction;
    return !SLF[CopyConstructor] && !SLF[MoveConstructor] && !SLF[Destructor] &&
           ranges::all_of(type.memberVariables(), [](auto* var) {
               return var->type()->hasTrivialLifetime();
           });
}

void InstContext::generateSLFs(StructType& type) {
    using enum SpecialLifetimeFunction;
    auto SLF = getDefinedSLFs(sym, type);
    bool const isDefaultConstructible = computeDefaultConstructible(type, SLF);
    bool const hasTrivialLifetime = computeTrivialLifetime(type, SLF);
    type.setIsDefaultConstructible(isDefaultConstructible);
    type.setHasTrivialLifetime(hasTrivialLifetime);
    if (isDefaultConstructible && !SLF[DefaultConstructor]) {
        bool anyMemberHasDefCtor = ranges::any_of(type.memberVariables(),
                                                  [](auto* var) {
            auto* type = dyncast<StructType const*>(var->type());
            return type && type->specialLifetimeFunction(DefaultConstructor);
        });
        if (anyMemberHasDefCtor) {
            SLF[DefaultConstructor] = generateSLF(DefaultConstructor, type);
        }
    }
    if (!hasTrivialLifetime) {
        if (!SLF[CopyConstructor]) {
            SLF[CopyConstructor] = generateSLF(CopyConstructor, type);
        }
        if (!SLF[MoveConstructor]) {
            SLF[MoveConstructor] = generateSLF(MoveConstructor, type);
        }
        if (!SLF[Destructor]) {
            SLF[Destructor] = generateSLF(Destructor, type);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
}

Function* InstContext::generateSLF(SpecialLifetimeFunction key,
                                   StructType& type) const {
    auto SMFKind = toSMF(key);
    Function* function = sym.withScopeCurrent(&type, [&] {
        return sym.declareFuncName(std::string(toString(SMFKind)));
    });
    SC_ASSERT(function, "Name can't be used by other symbol");
    bool result = sym.setFuncSig(function, makeLifetimeSignature(type, key));
    SC_ASSERT(result, "Can't be defined because we checked in getDefinedSLFs");
    function->setKind(FunctionKind::Generated);
    function->setIsMember();
    function->setSMFKind(SMFKind);
    type.addSpecialMemberFunction(SMFKind, function->overloadSet());
    return function;
}

FunctionSignature InstContext::makeLifetimeSignature(
    StructType& type, SpecialLifetimeFunction function) const {
    auto* self = sym.reference(QualType::Mut(&type));
    auto* rhs = sym.reference(QualType::Const(&type));
    auto* ret = sym.Void();
    using enum SpecialLifetimeFunction;
    switch (function) {
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
