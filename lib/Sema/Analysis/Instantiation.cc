#include "Sema/Analysis/Instantiation.h"

#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/DependencyGraph.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Entity.h"
#include "Sema/QualType.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    std::vector<StructureType const*> run();

    void instantiateStructureType(DependencyGraphNode&);
    void instantiateVariable(DependencyGraphNode&);
    void instantiateFunction(DependencyGraphNode&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition&) const;

    QualType analyzeParameter(ast::ParameterDeclaration&, size_t index) const;

    QualType analyzeTypeExpression(ast::Expression&) const;

    SymbolTable& sym;
    IssueHandler& iss;
    DependencyGraph& dependencyGraph;
    DependencyGraphNode const* currentNode = nullptr;
};

} // namespace

std::vector<StructureType const*> sema::instantiateEntities(
    SymbolTable& sym, IssueHandler& iss, DependencyGraph& typeDependencies) {
    Context ctx{ .sym = sym, .iss = iss, .dependencyGraph = typeDependencies };
    return ctx.run();
}

static bool isUserDefined(QualType type) {
    // clang-format off
    return SC_MATCH(*type) {
        [](StructureType const&) {
            return true;
        },
        [](ArrayType const& arrayType) {
            return isUserDefined(arrayType.elementType());
        },
        [](ObjectType const&) {
            return false;
        }
    }; // clang-format on
}

std::vector<StructureType const*> Context::run() {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    for (auto& node: dependencyGraph) {
        if (!isa<Variable>(node.entity)) {
            continue;
        }
        auto& var = cast<ast::VariableDeclaration&>(*node.astNode);
        QualType type = analyzeTypeExpression(*var.typeExpr());
        if (!type) {
            continue;
        }
        if (!isUserDefined(type)) {
            continue;
        }
        node.dependencies.push_back(
            utl::narrow_cast<u16>(dependencyGraph.index(type.get())));
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
                     ranges::to<utl::vector>;
        iss.push<StrongReferenceCycle>(std::move(nodes));
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
    std::vector<StructureType const*> sortedStructTypes;
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
    return sortedStructTypes;
}

static SpecialMemberFunction toSMF(ast::FunctionDefinition const& funcDef) {
    for (uint8_t i = 0; i < utl::to_underlying(SpecialMemberFunction::COUNT);
         ++i)
    {
        SpecialMemberFunction SMF{ i };
        if (toString(SMF) == funcDef.name()) {
            return SMF;
        }
    }
    return SpecialMemberFunction::COUNT;
}

void Context::instantiateStructureType(DependencyGraphNode& node) {
    ast::StructDefinition& structDef =
        cast<ast::StructDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize = 0;
    size_t objectAlign = 0;
    auto& structType = cast<StructureType&>(*structDef.entity());
    /// Here we collect all the variables and special member function of the
    /// struct
    for (auto [varIndex, varDecl]: structDef.body()->statements() |
                                       Filter<ast::VariableDeclaration> |
                                       ranges::views::enumerate)
    {
        auto& var = *varDecl->variable();
        structType.addMemberVariable(&var);
        if (!varDecl->type()) {
            return;
        }
        QualType varType = varDecl->type();
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
    structType.setSize(objectSize);
    structType.setAlign(objectAlign);
}

void Context::instantiateVariable(DependencyGraphNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    QualType type = analyzeTypeExpression(*varDecl.typeExpr());
    varDecl.decorate(node.entity, type);
    /// Here we set the TypeID of the variable in the symbol table.
    varDecl.variable()->setType(type);
}

static bool isExplicitRefTo(QualType argType, QualType referredType) {
    return isExplRef(argType) && stripReference(argType) == referredType;
}

void Context::instantiateFunction(DependencyGraphNode& node) {
    SC_ASSERT(isa<Function>(node.entity), "Must be a function");
    ast::FunctionDefinition& fnDecl =
        cast<ast::FunctionDefinition&>(*node.astNode);
    auto* function = cast<Function*>(node.entity);
    sym.makeScopeCurrent(function->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto result = sym.setSignature(function, analyzeSignature(fnDecl));
    if (!result) {
        if (auto* invStatement =
                dynamic_cast<InvalidStatement*>(result.error()))
        {
            invStatement->setStatement(fnDecl);
        }
        iss.push(result.error());
        return;
    }
    auto SMF = toSMF(fnDecl);
    if (SMF == SpecialMemberFunction::COUNT) {
        return;
    }
    auto pushError = [&] {
        using enum InvalidDeclaration::Reason;
        iss.push(new InvalidDeclaration(&fnDecl,
                                        InvalidSpecialMemberFunction,
                                        *function->parent()));
    };
    auto* structType = dyncast<StructureType*>(function->parent());
    if (!structType) {
        pushError();
        return;
    }
    function->setSMFKind(SMF);
    structType->addSpecialMemberFunction(SMF, function->overloadSet());
    auto const& sig = function->signature();
    if (sig.argumentCount() == 0 ||
        !isExplicitRefTo(sig.argumentType(0), structType))
    {
        pushError();
        return;
    }
    using enum SpecialMemberFunction;
    switch (function->SMFKind()) {
    case New:
        break;
    case Move:
        if (sig.argumentCount() != 2 ||
            !isExplicitRefTo(sig.argumentType(1), structType))
        {
            pushError();
            return;
        }
        break;
    case Delete:
        if (sig.argumentCount() != 1) {
            pushError();
            return;
        }
        break;
    case COUNT:
        SC_UNREACHABLE();
    }
}

FunctionSignature Context::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    utl::small_vector<QualType> argumentTypes;
    for (auto [index, param]: decl.parameters() | ranges::views::enumerate) {
        argumentTypes.push_back(analyzeParameter(*param, index));
    }
    /// For functions with unspecified return type we assume void until we
    /// implement return type deduction.
    QualType returnType = decl.returnTypeExpr() ?
                              analyzeTypeExpression(*decl.returnTypeExpr()) :
                              sym.Void();
    return FunctionSignature(std::move(argumentTypes), returnType);
}

QualType Context::analyzeParameter(ast::ParameterDeclaration& param,
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
    if (auto ref = thisParam->reference()) {
        return sym.explRef(QualType(structType, thisParam->mutability()));
    }
    return structType;
}

QualType Context::analyzeTypeExpression(ast::Expression& expr) const {
    DTorStack dtorStack;
    if (!sema::analyzeExpression(expr, dtorStack, sym, iss)) {
        return nullptr;
    }
    SC_ASSERT(dtorStack.empty(), "");
    if (!expr.isType()) {
        iss.push<BadSymbolReference>(expr, EntityCategory::Type);
        return nullptr;
    }
    return cast<ObjectType*>(expr.entity());
}
