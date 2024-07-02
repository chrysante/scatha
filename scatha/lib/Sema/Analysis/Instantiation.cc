#include "Sema/Analysis/Instantiation.h"

#include <array>
#include <vector>

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
using namespace ranges::views;

namespace {

struct InstContext {
    InstContext(sema::AnalysisContext& ctx):
        ctx(ctx), sym(ctx.symbolTable()), iss(ctx.issueHandler()) {}

    std::vector<RecordType const*> instantiateTypes(
        StructDependencyGraph& typeDependencies);

    void instantiateRecordType(SDGNode&);
    Type const* instantiateRecordMember(RecordType& parentType,
                                        ast::VariableDeclaration& varDecl,
                                        size_t declIndex);
    Type const* instantiateRecordMember(RecordType& parentType,
                                        ast::BaseClassDeclaration& decl,
                                        size_t declIndex);
    Type const* instantiateRecordMember(RecordType& parentType,
                                        ast::Declaration&, size_t);

    void instantiateNonStaticDataMember(SDGNode&);

    void instantiateBaseClass(SDGNode&);

    void instantiateFunction(ast::FunctionDefinition& def);

    void instantiateGlobalVariable(ast::VariableDeclaration& decl);

    FunctionType const* analyzeSignature(ast::FunctionDefinition& def) const;

    Type const* analyzeParam(ast::ParameterDeclaration&) const;

    Type const* analyzeThisParam(ast::ThisParameter&) const;

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
};

} // namespace

std::vector<RecordType const*> sema::instantiateEntities(
    AnalysisContext& ctx, StructDependencyGraph& typeDependencies,
    std::span<ast::Declaration*> globals) {
    InstContext instCtx(ctx);
    auto records = instCtx.instantiateTypes(typeDependencies);
    for (auto* decl: globals) {
        // clang-format off
        SC_MATCH(*decl) {
            [&](ast::FunctionDefinition& funcDef) {
                instCtx.instantiateFunction(funcDef);
            },
            [&](ast::VariableDeclaration& varDecl) {
                instCtx.instantiateGlobalVariable(varDecl);
            },
            [&](ast::Declaration&) {
                SC_UNREACHABLE();
            }
        }; // clang-format on
    }
    /// `structs` is topologically sorted, so each invocation of
    /// `declareSpecialLifetimeFunctions()` can assume that the types of all
    /// data members already have been analyzed for lifetime triviality
    for (auto* type: records | Filter<StructType>) {
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
    return records;
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

std::vector<RecordType const*> InstContext::instantiateTypes(
    StructDependencyGraph& dependencyGraph) {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    auto dataMembers = dependencyGraph | ranges::views::filter([](auto& node) {
        return isa<BaseClassObject>(node.entity) || isa<Variable>(node.entity);
    });
    for (auto& node: dataMembers) {
        // clang-format off
        auto* typeExpr = SC_MATCH (*node.astNode) {
            [](ast::VariableDeclaration& decl) { return decl.typeExpr(); },
            [](ast::BaseClassDeclaration& decl) { return decl.typeExpr(); },
            [](ast::ASTNode const&) -> ast::Expression* { SC_UNREACHABLE(); }
        }; // clang-format on
        auto* entity = sym.withScopeCurrent(node.entity->parent(), [&] {
            auto* expr = analyzeTypeExpr(typeExpr, ctx);
            return expr ? expr->entity() : nullptr;
        });
        if (isa<TypeDeductionQualifier>(entity)) {
            ctx.issue<BadTypeDeduction>(typeExpr, nullptr,
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
    std::vector<RecordType const*> sortedRecords;
    /// Instantiate all types and member variables.
    for (size_t index: dependencyTraversalOrder) {
        auto& node = dependencyGraph[index];
        // clang-format off
        SC_MATCH (*node.entity) {
            [&](Variable const&) { instantiateNonStaticDataMember(node); },
            [&](BaseClassObject const&) { instantiateBaseClass(node); },
            [&](RecordType const& type) {
                instantiateRecordType(node);
                sortedRecords.push_back(&type);
            },
            [&](Entity const&) { SC_UNREACHABLE(); }
        }; // clang-format on
    }
    return sortedRecords;
}

Type const* InstContext::instantiateRecordMember(
    RecordType& parentType, ast::VariableDeclaration& varDecl,
    size_t declIndex) {
    auto* structType = dyncast<StructType*>(&parentType);
    if (!structType) {
        SC_ASSERT(isa<ProtocolType>(parentType), "");
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::InProtocol);
        return nullptr;
    }
    auto& var = *varDecl.variable();
    structType->pushMemberVariable(&var);
    if (!varDecl.type()) {
        return nullptr;
    }
    auto* varType = varDecl.type();
    if (isa<ReferenceType>(varType)) {
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::RefInStruct, varType,
                              varDecl.initExpr());
        return nullptr;
    }
    if (isa<ProtocolType>(varType)) {
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::ProtocolType, varType,
                              varDecl.initExpr());
        return nullptr;
    }
    if (!varType->isComplete()) {
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::IncompleteType, varType,
                              varDecl.initExpr());
        return nullptr;
    }
    if (auto* array = dyncast<ArrayType const*>(varType)) {
        const_cast<ArrayType*>(array)->recomputeSize();
    }
    var.setIndex(declIndex);
    return varType;
}

Type const* InstContext::instantiateRecordMember(
    RecordType& parentType, ast::BaseClassDeclaration& decl, size_t declIndex) {
    auto& obj = *decl.object();
    parentType.pushBaseObject(&obj);
    auto* baseType = decl.type();
    if (!baseType) {
        return nullptr;
    }
    if (isa<ProtocolType>(parentType) && !isa<ProtocolType>(baseType)) {
        ctx.issue<BadBaseDecl>(&decl, BadBaseDecl::NotAProtocol, decl.type());
        return nullptr;
    }
    if (isa<StructType>(parentType) && !isa<RecordType>(baseType)) {
        ctx.issue<BadBaseDecl>(&decl, BadBaseDecl::InvalidType, decl.type());
        return nullptr;
    }
    obj.setIndex(declIndex);
    return baseType;
}

Type const* InstContext::instantiateRecordMember(RecordType&, ast::Declaration&,
                                                 size_t) {
    SC_UNREACHABLE();
}

void InstContext::instantiateRecordType(SDGNode& node) {
    auto& recordDef = cast<ast::RecordDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize = 0;
    size_t objectAlign = 0;
    auto& recordType = cast<RecordType&>(*recordDef.entity());
    auto decls =
        concat(recordDef.baseClasses() | transform(cast<ast::Declaration*>),
               recordDef.body()->statements() |
                   Filter<ast::VariableDeclaration>);
    /// Here we collect all the base classes and members of the record
    for (auto [declIndex, decl]: decls | ranges::views::enumerate) {
        if (!decl->isDecorated()) {
            continue;
        }
        auto* type = visit(*decl, [&, declIndex = declIndex](auto& decl) {
            return instantiateRecordMember(recordType, decl, declIndex);
        });
        if (!type || !type->isComplete()) {
            continue;
        }
        objectAlign = std::max(objectAlign, type->align());
        SC_ASSERT(type->size() % type->align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, type->align());
        objectSize += type->size();
    }
    if (objectAlign > 0) {
        objectSize = utl::round_up(objectSize, objectAlign);
    }
    if (auto* structType = dyncast<StructType*>(&recordType)) {
        structType->setSize(std::max(objectSize, size_t{ 1 }));
        structType->setAlign(std::max(objectAlign, size_t{ 1 }));
    }
    else {
        SC_ASSERT(objectSize == 0, "");
        SC_ASSERT(objectAlign == 0, "");
    }
}

static Type const* getType(ast::Expression const* expr) {
    if (!expr || !expr->isDecorated() || !expr->isType()) {
        return nullptr;
    }
    return dyncast<Type const*>(expr->entity());
}

void InstContext::instantiateNonStaticDataMember(SDGNode& node) {
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

void InstContext::instantiateBaseClass(SDGNode& node) {
    ast::BaseClassDeclaration& decl =
        cast<ast::BaseClassDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto* type = getType(decl.typeExpr());
    /// Here we set the type of the variable in the symbol table.
    auto* obj = cast<BaseClassObject*>(node.entity);
    sym.setBaseClassType(obj, type);
    decl.decorateDecl(obj);
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

/// Analyzes the type expression \p typeExpr and ensures it is not a type
/// deduction qualifier
static Type const* analyzeGlobalTypeExpr(ast::Expression* typeExpr,
                                         AnalysisContext& ctx) {
    if (!(typeExpr = analyzeTypeExpr(typeExpr, ctx))) {
        return nullptr;
    }
    if (isa<TypeDeductionQualifier>(typeExpr->entity())) {
        ctx.issue<BadTypeDeduction>(typeExpr, nullptr,
                                    BadTypeDeduction::InvalidContext);
        return nullptr;
    }
    return cast<Type const*>(typeExpr->entity());
}

FunctionType const* InstContext::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    auto argumentTypes = decl.parameters() |
                         ranges::views::transform([&](auto* param) {
        return analyzeParam(*param);
    }) | ToSmallVector<>;
    auto* retType = analyzeGlobalTypeExpr(decl.returnTypeExpr(), ctx);
    /// If the return type is not specified it will be deduced during function
    /// analysis
    if (!retType) {
        return sym.functionType(argumentTypes, nullptr);
    }
    if (!retType->isCompleteOrVoid()) {
        ctx.issue<BadPassedType>(decl.returnTypeExpr(), BadPassedType::Return);
    }
    return sym.functionType(argumentTypes, retType);
}

Type const* InstContext::analyzeParam(ast::ParameterDeclaration& param) const {
    if (auto* thisParam = dyncast<ast::ThisParameter*>(&param)) {
        return analyzeThisParam(*thisParam);
    }
    auto* type = analyzeGlobalTypeExpr(param.typeExpr(), ctx);
    if (!type) {
        return nullptr;
    }
    if (!type->isComplete()) {
        ctx.issue<BadPassedType>(param.typeExpr(), BadPassedType::Argument);
    }
    return type;
}

Type const* InstContext::analyzeThisParam(ast::ThisParameter& param) const {
    auto* record = param.findAncestor<ast::RecordDefinition>();
    if (!record) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisInFreeFunction);
        return nullptr;
    }
    if (param.index() != 0) {
        ctx.issue<BadVarDecl>(&param, BadVarDecl::ThisPosition);
        return nullptr;
    }
    auto* recordType = cast<RecordType const*>(record->entity());
    if (param.isReference()) {
        auto bindMode = isa<ProtocolType>(recordType) ?
                            PointerBindMode::Dynamic :
                            param.bindMode();
        return sym.reference(
            QualType(recordType, param.mutability(), bindMode));
    }
    return recordType;
}

void InstContext::instantiateGlobalVariable(ast::VariableDeclaration& decl) {
    auto* var = cast<Variable*>(decl.object());
    sym.withScopeCurrent(var->parent(), [&] {
        auto* type = analyzeGlobalTypeExpr(decl.typeExpr(), ctx);
        sym.setVariableType(var, type);
        if (isa<ReferenceType>(type)) {
            ctx.issue<BadVarDecl>(&decl,
                                  BadVarDecl::GlobalReferencesNotYetSupported);
        }
    });
}
