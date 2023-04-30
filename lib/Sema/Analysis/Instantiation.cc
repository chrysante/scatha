#include "Sema/Analysis/Instantiation.h"

#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    void run();

    void instantiateStructureType(DependencyGraphNode&);
    void instantiateVariable(DependencyGraphNode&);
    void instantiateFunction(DependencyGraphNode&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition&) const;

    QualType const* analyzeParameter(ast::ParameterDeclaration&,
                                     size_t index) const;

    QualType const* analyzeTypeExpression(ast::Expression&) const;

    SymbolTable& sym;
    IssueHandler& iss;
    DependencyGraph& dependencyGraph;
    DependencyGraphNode const* currentNode = nullptr;
};

} // namespace

void sema::instantiateEntities(SymbolTable& sym,
                               IssueHandler& iss,
                               DependencyGraph& typeDependencies) {
    Context ctx{ .sym = sym, .iss = iss, .dependencyGraph = typeDependencies };
    ctx.run();
}

static bool isBuiltin(ObjectType const* type) {
    auto* sType = dyncast<StructureType const*>(type);
    return sType && sType->isBuiltin();
}

void Context::run() {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    for (auto& node: dependencyGraph) {
        if (!isa<Variable>(node.entity)) {
            continue;
        }
        auto& var        = cast<ast::VariableDeclaration&>(*node.astNode);
        auto const* type = analyzeTypeExpression(*var.typeExpr());
        if (!type) {
            continue;
        }
        if (isBuiltin(type->base())) {
            continue;
        }
        node.dependencies.push_back(
            utl::narrow_cast<u16>(dependencyGraph.index(type->base())));
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
        using Node = StrongReferenceCycle::Node;
        auto nodes = cycle | ranges::views::transform([&](size_t index) {
                         auto const& node = dependencyGraph[index];
                         return Node{ node.astNode, node.entity };
                     }) |
                     ranges::to<utl::vector<Node>>;
        iss.push<StrongReferenceCycle>(std::move(nodes));
        return;
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
    utl::small_vector<StructureType*> sortedStructTypes;
    /// Instantiate all types and member variables.
    for (size_t const index: dependencyTraversalOrder) {
        auto& node = dependencyGraph[index];
        // clang-format off
        visit(*node.entity, utl::overload{
            [&](Variable const&) { instantiateVariable(node); },
            [&](StructureType& type) {
                instantiateStructureType(node);
                sortedStructTypes.push_back(&type);
            },
            [&](Function const&) { instantiateFunction(node); },
            [&](Entity const&) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
    sym.setSortedStructureTypes(std::move(sortedStructTypes));
}

void Context::instantiateStructureType(DependencyGraphNode& node) {
    ast::StructDefinition& structDef =
        cast<ast::StructDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize  = 0;
    size_t objectAlign = 0;
    auto& objectType   = cast<StructureType&>(*structDef.entity());
    for (size_t index = 0; auto* statement: structDef.body()->statements()) {
        if (statement->nodeType() != ast::NodeType::VariableDeclaration) {
            continue;
        }
        auto& varDecl = cast<ast::VariableDeclaration&>(*statement);
        auto& var     = *varDecl.variable();
        objectType.addMemberVariable(&var);
        if (!varDecl.type()) {
            break;
        }
        auto const* objType = varDecl.type()->base();
        SC_ASSERT(objType->isComplete(),
                  "Type should be complete at this stage");
        objectAlign = std::max(objectAlign, objType->align());
        SC_ASSERT(objType->size() % objType->align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, objType->align());
        size_t const currentOffset = objectSize;
        varDecl.setOffset(currentOffset);
        varDecl.setIndex(index);
        var.setOffset(currentOffset);
        var.setIndex(index);
        objectSize += objType->size();
        ++index;
    }
    objectType.setSize(objectSize);
    objectType.setAlign(objectAlign);
}

void Context::instantiateVariable(DependencyGraphNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto* type = analyzeTypeExpression(*varDecl.typeExpr());
    varDecl.decorate(node.entity, type);
    /// Here we set the TypeID of the variable in the symbol table.
    varDecl.variable()->setType(type);
}

void Context::instantiateFunction(DependencyGraphNode& node) {
    SC_ASSERT(isa<Function>(node.entity), "Must be a function");
    ast::FunctionDefinition& fnDecl =
        cast<ast::FunctionDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.scope);
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto signature                  = analyzeSignature(fnDecl);
    auto result =
        sym.setSignature(cast<Function*>(node.entity), std::move(signature));
    if (!result) {
        if (auto* invStatement =
                dynamic_cast<InvalidStatement*>(result.error()))
        {
            invStatement->setStatement(fnDecl);
        }
        iss.push(result.error());
        return;
    }
}

FunctionSignature Context::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    utl::small_vector<QualType const*> argumentTypes;
    for (auto [index, param]: decl.parameters() | ranges::views::enumerate) {
        argumentTypes.push_back(analyzeParameter(*param, index));
    }
    /// For functions with unspecified return type we assume void until we
    /// implement return type deduction.
    QualType const* returnType =
        decl.returnTypeExpr() ? analyzeTypeExpression(*decl.returnTypeExpr()) :
                                sym.qualify(sym.Void());
    return FunctionSignature(std::move(argumentTypes), returnType);
}

QualType const* Context::analyzeParameter(ast::ParameterDeclaration& param,
                                          size_t index) const {
    auto* thisParam = dyncast<ast::ThisParameter const*>(&param);
    if (!thisParam) {
        return analyzeTypeExpression(*param.typeExpr());
    }
    if (index != 0) {
        iss.push<InvalidDeclaration>(&param,
                                     InvalidDeclaration::Reason::ThisParameter,
                                     sym.currentScope());
        return nullptr;
    }
    auto* structure = dyncast<ast::StructDefinition const*>(
        param.parent()->parent()->parent());
    if (!structure) {
        iss.push<InvalidDeclaration>(&param,
                                     InvalidDeclaration::Reason::ThisParameter,
                                     sym.currentScope());
        return nullptr;
    }
    auto* structType = cast<StructureType const*>(structure->entity());
    return sym.qualify(structType, thisParam->qualifiers());
}

QualType const* Context::analyzeTypeExpression(ast::Expression& expr) const {
    if (!sema::analyzeExpression(expr, sym, iss)) {
        return nullptr;
    }
    if (!expr.isType()) {
        iss.push<BadSymbolReference>(expr, EntityCategory::Type);
        return nullptr;
    }
    return cast<QualType const*>(expr.entity());
}
