#include "Sema/Analysis/Instantiation.h"

#include <array>

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
#include "Sema/LifetimeFunctionAnalysis.h"
#include "Sema/QualType.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct InstContext {
    InstContext(sema::AnalysisContext& ctx):
        ctx(ctx), sym(ctx.symbolTable()), iss(ctx.issueHandler()) {}

    utl::vector<StructType const*> instantiateTypes(
        StructDependencyGraph& typeDependencies);

    void instantiateFunctions(std::span<ast::FunctionDefinition*> functions);

    void instantiateStructureType(SDGNode&);

    void instantiateVariable(SDGNode&);

    void instantiateFunction(ast::FunctionDefinition& def);

    FunctionType const* analyzeSignature(ast::FunctionDefinition& def) const;

    Type const* analyzeParam(ast::ParameterDeclaration&) const;

    Type const* analyzeThisParam(ast::ThisParameter&) const;

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
};

} // namespace

utl::vector<StructType const*> sema::instantiateEntities(
    AnalysisContext& ctx, StructDependencyGraph& typeDependencies,
    std::span<ast::FunctionDefinition*> functions) {
    InstContext instCtx(ctx);
    auto structs = instCtx.instantiateTypes(typeDependencies);
    for (auto* def: functions) {
        instCtx.instantiateFunction(*def);
    }
    /// `structs` is topologically sorted, so each invocation of
    /// `declareSpecialLifetimeFunctions()` can assume that the types of all
    /// data members already have been analyzed for lifetime triviality
    for (auto* type: structs) {
        /// Const cast is fine because we just have the pointers as const to
        /// return them soon, they all have mutable origin.
        auto* mutType = const_cast<StructType*>(type);
        /// Members of array type may not have lifetime analyzed here
        for (auto* member: mutType->members() | Filter<ObjectType>) {
            if (!member->hasLifetimeMetadata()) {
                analyzeLifetime(const_cast<ObjectType&>(*member),
                                ctx.symbolTable());
            }
        }
        analyzeLifetime(*mutType, ctx.symbolTable());
    }
    ctx.symbolTable().analyzeMissingLifetimes();
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

/// \Returns `true` if \p entity is defined in a library and thus already
/// complete
static bool isLibEntity(Entity const* entity) {
    auto* scope = entity->parent();
    while (scope) {
        if (isa<Library>(scope)) {
            return true;
        }
        scope = scope->parent();
    }
    return false;
}

static Type const* stripArray(Type const* type) {
    if (auto* array = dyncast<ArrayType const*>(type)) {
        return array->elementType();
    }
    return type;
}

utl::vector<StructType const*> InstContext::instantiateTypes(
    StructDependencyGraph& dependencyGraph) {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    auto dataMembers = dependencyGraph | ranges::views::filter([](auto& node) {
        return isa<Variable>(node.entity);
    });
    for (auto& node: dataMembers) {
        auto& var = cast<ast::VariableDeclaration&>(*node.astNode);
        auto* entity = sym.withScopeCurrent(node.entity->parent(), [&] {
            auto* expr = analyzeTypeExpr(var.typeExpr(), ctx);
            return expr ? expr->entity() : nullptr;
        });
        if (isa<TypeDeductionQualifier>(entity)) {
            ctx.issue<BadTypeDeduction>(var.typeExpr(), nullptr,
                                        BadTypeDeduction::InvalidContext);
            continue;
        }
        auto* type = cast<Type const*>(entity);
        if (type && isUserDefined(type) && !isLibEntity(type)) {
            /// Because type instantiations depend on the element type, but
            /// array types are not in the dependency graph, we strip array
            /// types here
            size_t index = dependencyGraph.index(stripArray(type));
            node.dependencies.push_back(utl::narrow_cast<u16>(index));
        }
    }
    /// Check for cycles
    auto indices = ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
                   ranges::to<utl::small_vector<u16>>;
    auto const cycle = utl::find_cycle(indices.begin(), indices.end(),
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
        structType.pushMemberVariable(&var);
        if (!varDecl->type()) {
            continue;
        }
        if (isa<ReferenceType>(varDecl->type())) {
            ctx.issue<BadVarDecl>(varDecl, BadVarDecl::RefInStruct,
                                  varDecl->type(), varDecl->initExpr());
            continue;
        }
        auto* varType = varDecl->type();
        if (auto* array = dyncast<ArrayType const*>(varType)) {
            const_cast<ArrayType*>(array)->recomputeSize();
        }
        objectAlign = std::max(objectAlign, varType->align());
        SC_ASSERT(varType->size() % varType->align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, varType->align());
        var.setIndex(varIndex);
        objectSize += varType->size();
    }
    if (objectAlign > 0) {
        objectSize = utl::round_up(objectSize, objectAlign);
    }
    structType.setSize(std::max(objectSize, size_t{ 1 }));
    structType.setAlign(std::max(objectAlign, size_t{ 1 }));
}

static Type const* getType(ast::Expression const* expr) {
    if (!expr || !expr->isDecorated() || !expr->isType()) {
        return nullptr;
    }
    return dyncast<Type const*>(expr->entity());
}

void InstContext::instantiateVariable(SDGNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto* type = getType(varDecl.typeExpr());
    /// Here we set the TypeID of the variable in the symbol table.
    auto* var = cast<Variable*>(node.entity);
    sym.setVariableType(var, type);
    varDecl.decorateVarDecl(var);
}

void InstContext::instantiateFunction(ast::FunctionDefinition& def) {
    auto* F = def.function();
    sym.makeScopeCurrent(F->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto result = sym.setFunctionType(F, analyzeSignature(def));
    if (!result) {
        return;
    }
    if (def.externalLinkage()) {
        if (*def.externalLinkage() != "C") {
            ctx.issue<BadFuncDef>(&def, BadFuncDef::UnknownLinkage);
        }
        else {
            F->setKind(FunctionKind::Foreign);
        }
    }
}

FunctionType const* InstContext::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    auto argumentTypes = decl.parameters() |
                         ranges::views::transform([&](auto* param) {
        return analyzeParam(*param);
    }) | ToSmallVector<>;
    /// If the return type is not specified it will be deduced during function
    /// analysis
    auto* retExpr = analyzeTypeExpr(decl.returnTypeExpr(), ctx);
    if (!retExpr) {
        return sym.functionType(argumentTypes, nullptr);
    }
    if (isa<TypeDeductionQualifier>(retExpr->entity())) {
        ctx.issue<BadTypeDeduction>(retExpr, nullptr,
                                    BadTypeDeduction::InvalidContext);
        return sym.functionType(argumentTypes, nullptr);
    }
    auto* retType = cast<Type const*>(retExpr->entity());
    if (!retType->isCompleteOrVoid()) {
        ctx.issue<BadPassedType>(decl.returnTypeExpr(), BadPassedType::Return);
    }
    return sym.functionType(argumentTypes, retType);
}

Type const* InstContext::analyzeParam(ast::ParameterDeclaration& param) const {
    if (auto* thisParam = dyncast<ast::ThisParameter*>(&param)) {
        return analyzeThisParam(*thisParam);
    }
    auto* typeExpr = analyzeTypeExpr(param.typeExpr(), ctx);
    if (!typeExpr) {
        return nullptr;
    }
    if (isa<TypeDeductionQualifier>(typeExpr->entity())) {
        ctx.issue<BadTypeDeduction>(typeExpr, nullptr,
                                    BadTypeDeduction::InvalidContext);
        return nullptr;
    }
    auto* type = cast<Type const*>(typeExpr->entity());
    if (!type->isComplete()) {
        ctx.issue<BadPassedType>(typeExpr, BadPassedType::Argument);
    }
    return type;
}

Type const* InstContext::analyzeThisParam(ast::ThisParameter& param) const {
    auto* structure = param.findAncestor<ast::StructDefinition>();
    if (!structure) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisInFreeFunction);
        return nullptr;
    }
    if (param.index() != 0) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisPosition);
        return nullptr;
    }
    auto* structType = cast<StructType const*>(structure->entity());
    if (param.isReference()) {
        return sym.reference(QualType(structType, param.mutability()));
    }
    return structType;
}
