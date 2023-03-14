#include "Sema/Analysis/Instantiation.h"

#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    void run();

    void instantiateObjectType(DependencyGraphNode const&);
    void instantiateVariable(DependencyGraphNode const&);
    void instantiateFunction(DependencyGraphNode const&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition const&) const;

    TypeID analyzeTypeExpression(ast::Expression&) const;

    SymbolTable& sym;
    issue::SemaIssueHandler& iss;
    DependencyGraph& dependencyGraph;
    DependencyGraphNode const* currentNode = nullptr;
};

} // namespace

void sema::instantiateEntities(SymbolTable& sym,
                               issue::SemaIssueHandler& iss,
                               DependencyGraph& typeDependencies) {
    Context ctx{ .sym = sym, .iss = iss, .dependencyGraph = typeDependencies };
    ctx.run();
}

void Context::run() {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    for (auto& node: dependencyGraph) {
        if (node.category != SymbolCategory::Variable) {
            continue;
        }
        auto& var = cast<ast::VariableDeclaration const&>(*node.astNode);
        auto const typeID = analyzeTypeExpression(*var.typeExpr);
        auto const& type  = sym.getObjectType(typeID);
        if (type.isBuiltin()) {
            continue;
        }
        node.dependencies.push_back(
            utl::narrow_cast<u16>(dependencyGraph.index(typeID)));
    }
    /// Check for cycles
    auto indices = ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
                   ranges::to<utl::small_vector<u16>>;
    auto const cycle = utl::find_cycle(
        indices.begin(),
        indices.end(),
        [&](size_t index) -> auto const& {
            return dependencyGraph[index].dependencies;
        });
    if (!cycle.empty()) {
        using Node = StrongReferenceCycle::Node;
        auto nodes = cycle | ranges::views::transform([&](size_t index) {
                         auto const& node = dependencyGraph[index];
                         return Node{ node.astNode, node.symbolID };
                     }) |
                     ranges::to<utl::vector<Node>>;
        iss.push(SemanticIssue{ StrongReferenceCycle(std::move(nodes)) });
        return;
    }
    /// Sort dependencies...
    auto dependencyTraversalOrder =
        ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
        ranges::to<utl::small_vector<u16>>;
    utl::topsort(
        dependencyTraversalOrder.begin(),
        dependencyTraversalOrder.end(),
        [&](size_t index) -> auto const& {
            return dependencyGraph[index].dependencies;
        });
    utl::small_vector<TypeID> sortedObjTypes;
    /// Instantiate all types and member variables.
    for (size_t const index: dependencyTraversalOrder) {
        auto const& node = dependencyGraph[index];
        switch (node.category) {
        case SymbolCategory::Variable: instantiateVariable(node); break;
        case SymbolCategory::ObjectType:
            instantiateObjectType(node);
            sortedObjTypes.push_back(TypeID(node.symbolID));
            break;
        case SymbolCategory::Function: instantiateFunction(node); break;
        default: break;
        }
    }
    sym.setSortedObjectTypes(std::move(sortedObjTypes));
}

void Context::instantiateObjectType(DependencyGraphNode const& node) {
    ast::StructDefinition& structDef =
        cast<ast::StructDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize  = 0;
    size_t objectAlign = 0;
    auto& objectType   = sym.getObjectType(structDef.symbolID());
    for (auto&& [index, statement]:
         structDef.body->statements | ranges::views::enumerate)
    {
        if (statement->nodeType() != ast::NodeType::VariableDeclaration) {
            continue;
        }
        auto& varDecl = cast<ast::VariableDeclaration&>(*statement);
        objectType.addMemberVariable(varDecl.symbolID());
        if (varDecl.typeID() == TypeID::Invalid) {
            break;
        }
        auto const& type = sym.getObjectType(varDecl.typeID());
        SC_ASSERT(type.isComplete(), "Type should be complete at this stage");
        objectAlign = std::max(objectAlign, type.align());
        SC_ASSERT(type.size() % type.align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, type.align());
        size_t const currentOffset = objectSize;
        varDecl.setOffset(currentOffset);
        varDecl.setIndex(index);
        auto& var = sym.getVariable(varDecl.symbolID());
        var.setOffset(currentOffset);
        var.setIndex(index);
        objectSize += type.size();
    }
    objectType.setSize(objectSize);
    objectType.setAlign(objectAlign);
}

void Context::instantiateVariable(DependencyGraphNode const& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    TypeID const typeID             = analyzeTypeExpression(*varDecl.typeExpr);
    varDecl.decorate(node.symbolID, typeID);
    /// Here we set the TypeID of the variable in the symbol table.
    auto& var = sym.getVariable(varDecl.symbolID());
    var.setTypeID(typeID);
}

void Context::instantiateFunction(DependencyGraphNode const& node) {
    SC_ASSERT(node.category == SymbolCategory::Function, "Must be a function");
    ast::FunctionDefinition& fnDecl =
        cast<ast::FunctionDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto signature                  = analyzeSignature(fnDecl);
    auto result = sym.setSignature(node.symbolID, std::move(signature));
    if (!result) {
        result.error().setStatement(fnDecl);
        iss.push(result.error());
        return;
    }
}

FunctionSignature Context::analyzeSignature(
    ast::FunctionDefinition const& decl) const {
    utl::small_vector<TypeID> argumentTypes;
    for (auto& param: decl.parameters) {
        argumentTypes.push_back(analyzeTypeExpression(*param->typeExpr));
    }
    /// For functions with unspecified return type we assume void until we
    /// implement return type deduction.
    TypeID const returnTypeID =
        decl.returnTypeExpr ? analyzeTypeExpression(*decl.returnTypeExpr) :
                              sym.Void();
    return FunctionSignature(std::move(argumentTypes), returnTypeID);
}

TypeID Context::analyzeTypeExpression(ast::Expression& expr) const {
    auto const typeExprResult = sema::analyzeExpression(expr, sym, iss);
    if (!typeExprResult) {
        return TypeID::Invalid;
    }
    if (typeExprResult.category() != ast::EntityCategory::Type) {
        iss.push(BadSymbolReference(expr,
                                    expr.entityCategory(),
                                    ast::EntityCategory::Type));
        return TypeID::Invalid;
    }
    auto const& type = sym.getObjectType(typeExprResult.typeID());
    return type.symbolID();
}
