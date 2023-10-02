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

    void instantiateFunction(ast::FunctionDefinition&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition&) const;

    Type const* analyzeParam(ast::ParameterDeclaration&) const;

    Type const* analyzeThisParam(ast::ThisParameter&) const;

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
    /// `declareSpecialLifetimeFunctions()` can assume that the types of all
    /// data members already have been analyzed for lifetime triviality
    for (auto* type: structs) {
        /// Const cast is fine because we just have the pointers as const to
        /// return them soon, they all have mutable origin.
        auto* mutType = const_cast<StructType*>(type);
        declareSpecialLifetimeFunctions(*mutType, ctx.symbolTable());
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
        auto* type = sym.withScopeCurrent(node.entity->parent(), [&] {
            return analyzeTypeExpr(var.typeExpr(), ctx);
        });
        if (type && isUserDefined(type)) {
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
    structType.setSize(std::max(objectSize, size_t{ 1 }));
    structType.setAlign(std::max(objectAlign, size_t{ 1 }));
}

static Type const* getType(ast::Expression const* expr) {
    if (!expr || !expr->isDecorated() || !expr->isType()) {
        return nullptr;
    }
    return cast<Type const*>(expr->entity());
}

void InstContext::instantiateVariable(SDGNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto* type = getType(varDecl.typeExpr());
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
    auto argumentTypes = decl.parameters() |
                         ranges::views::transform([&](auto* param) {
                             return analyzeParam(*param);
                         }) |
                         ToSmallVector<>;
    /// If the return type is not specified it will be deduced during function
    /// analysis
    auto* returnType = decl.returnTypeExpr() ?
                           analyzeTypeExpr(decl.returnTypeExpr(), ctx) :
                           nullptr;
    if (returnType && returnType != sym.Void() && !returnType->isComplete()) {
        ctx.issue<BadPassedType>(decl.returnTypeExpr(), BadPassedType::Return);
    }
    return FunctionSignature(std::move(argumentTypes), returnType);
}

Type const* InstContext::analyzeParam(ast::ParameterDeclaration& param) const {
    if (auto* thisParam = dyncast<ast::ThisParameter*>(&param)) {
        return analyzeThisParam(*thisParam);
    }
    auto* type = analyzeTypeExpr(param.typeExpr(), ctx);
    if (!type) {
        return nullptr;
    }
    if (!type->isComplete()) {
        ctx.issue<BadPassedType>(param.typeExpr(), BadPassedType::Argument);
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
