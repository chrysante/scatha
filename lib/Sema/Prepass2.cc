#include "Sema/Prepass2.h"

#include <utl/vector.hpp>

#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "AST/Visit.h"
#include "Sema/ExpressionAnalysis.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace scatha::sema;

namespace {

/// Gathers all declarations and declares them in the symbol table. Also analyzes the dependencies of structs because they are trivial.
struct Context {
    /// Dispatches to the appropriate one of the \p gather() overloads below based on the runtime type of \p node
    size_t dispatch(ast::AbstractSyntaxTree& node);

    size_t gather(ast::TranslationUnit&);
    size_t gather(ast::FunctionDefinition&);
    size_t gather(ast::StructDefinition&);
    size_t gather(ast::VariableDeclaration&);
    size_t gather(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    void analyzeRemainingDependencies();
    void analyzeFunctionDecl(DependencyGraphNode&);
    void analyzeVariableDecl(DependencyGraphNode&);
    
    TypeID analyzeTypeExpression(ast::Expression&);
    ExpressionAnalysisResult analyzeExpression(ast::Expression&);
    
    void addTypeDependency(DependencyGraphNode&, TypeID) const;
    
    SymbolTable& sym;
    issue::IssueHandler& iss;
    utl::vector<DependencyGraphNode>& dependencyGraph;
    utl::hashmap<SymbolID, std::size_t> graphIndices{};
};

} // namespace

std::tuple<SymbolTable, utl::vector<DependencyGraphNode>> scatha::sema::prepass2(ast::AbstractSyntaxTree& root,
                                                                                 issue::IssueHandler& iss)
{
    SymbolTable sym;
    utl::vector<DependencyGraphNode> dependencyGraph;
    Context ctx{ sym, iss, dependencyGraph };
    ctx.dispatch(root);
    ctx.analyzeRemainingDependencies();
    return { std::move(sym), std::move(dependencyGraph) };
}

size_t Context::dispatch(ast::AbstractSyntaxTree& node) {
    return visit(node, [this](auto& node) { return this->gather(node); });
}

size_t Context::gather(ast::TranslationUnit& tu) {
    for (auto& decl : tu.declarations) {
        dispatch(*decl);
    }
    return invalidIndex;
}

size_t Context::gather(ast::FunctionDefinition& fn) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push(InvalidDeclaration(&fn,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::Function));
        return (size_t)-1;
    }
    Expected const overloadSet = sym.declareFunction(fn.token());
    if (!overloadSet.hasValue()) {
        auto error = overloadSet.error();
        error.setStatement(fn);
        iss.push(error);
        return invalidIndex;
    }
    fn.overloadSetID = overloadSet->symbolID();
    size_t const index = dependencyGraph.size();
    dependencyGraph.push_back({
        .symbolID = SymbolID::Invalid,
        .category = SymbolCategory::Function,
        .astNode = &fn
    });
    return index;
}

size_t Context::gather(ast::StructDefinition& s) {
    if (auto const sk = sym.currentScope().kind();
        sk != ScopeKind::Global && sk != ScopeKind::Namespace && sk != ScopeKind::Object) {
        /// Struct defintion is only allowed in the global scope, at namespace
        /// scope and structure scope
        iss.push(InvalidDeclaration(&s,
                                    InvalidDeclaration::Reason::InvalidInCurrentScope,
                                    sym.currentScope(),
                                    SymbolCategory::ObjectType));
        return invalidIndex;
    }
    Expected const objType = sym.declareObjectType(s.token());
    if (!objType) {
        auto error = objType.error();
        error.setStatement(s);
        iss.push(error);
        return invalidIndex;
    }
    s.symbolID = objType->symbolID();
    /// After we declared this type we gather all its members
    utl::small_vector<u16> dependencies;
    sym.pushScope(objType->symbolID());
    utl::armed_scope_guard popScope = [&] { sym.popScope(); };
    for (auto& statement: s.body->statements) {
        if (statement->nodeType() == ast::NodeType::VariableDeclaration) {
            continue;
        }
        size_t const index = dispatch(*statement);
        dependencies.push_back(utl::narrow_cast<u16>(index));
    }
    size_t const index = dependencyGraph.size();
    auto const [_, insertSuccess] = graphIndices.insert({ objType->symbolID(), index });
    SC_ASSERT(insertSuccess, "Same symbol ID cannot exist twice");
    dependencyGraph.push_back({
        .symbolID = objType->symbolID(),
        .category = SymbolCategory::ObjectType,
        .astNode = &s,
        .dependencies = dependencies
    });
    return index;
}

size_t Context::gather(ast::VariableDeclaration& var) {
    SC_ASSERT(sym.currentScope().kind() == ScopeKind::Object,
              "We only want to prepass struct definitions. What are we doing here?");
    SC_ASSERT(var.typeExpr,
              "In structs variables need explicit type "
              "specifiers. Make this a program issue.");
    auto declResult = sym.declareVariable(var.token());
    if (!declResult) {
        auto error = declResult.error();
        error.setStatement(var);
        iss.push(error);
        return invalidIndex;
    }
    size_t const index = dependencyGraph.size();
    dependencyGraph.push_back({
        .symbolID = SymbolID::Invalid,
        .category = SymbolCategory::Variable,
        .astNode = &var
    });
    return index;
}

void Context::analyzeRemainingDependencies() {
    for (auto& node: dependencyGraph) {
        switch (node.category) {
        case SymbolCategory::Function: {
            analyzeFunctionDecl(node);
            break;
        }
        case SymbolCategory::ObjectType: {
            /// Nothing to do here
            break;
        }
        case SymbolCategory::Variable: {
            analyzeVariableDecl(node);
            break;
        }
        default: SC_UNREACHABLE();
        }
    }
}

void Context::analyzeFunctionDecl(DependencyGraphNode& node) {
    SC_ASSERT(node.category == SymbolCategory::Function, "Must be a function");
    ast::FunctionDefinition& decl = utl::down_cast<ast::FunctionDefinition&>(*node.astNode);
    /// Analyze the argument expressions
    utl::small_vector<TypeID> argumentTypes;
    for (auto& param: decl.parameters) {
        argumentTypes.push_back(analyzeTypeExpression(*param->typeExpr));
    }
    /// Analyze the return type expression
    TypeID const returnTypeID = analyzeTypeExpression(*decl.returnTypeExpr);
    /// Might be \p TypeID::Invalid if we are in the last pass but we still declare
    /// the function and go on
    decl.returnTypeID = returnTypeID;
    Expected func   = sym.addFunction(decl.overloadSetID,
                                      FunctionSignature(argumentTypes, returnTypeID));
    if (!func.hasValue()) {
        auto error = func.error();
        error.setStatement(decl);
        iss.push(error);
        return;
    }
    addTypeDependency(node, returnTypeID);
    for (auto typeID: argumentTypes) {
        addTypeDependency(node, typeID);
    }
    /// Decorate the AST node
    decl.symbolID            = func->symbolID();
    decl.functionTypeID      = func->typeID();
    decl.body->scopeKind     = ScopeKind::Function;
    decl.body->scopeSymbolID = decl.symbolID;
}

void Context::analyzeVariableDecl(DependencyGraphNode& node) {
    SC_ASSERT(node.category == SymbolCategory::Variable, "Must be a variable");
    ast::VariableDeclaration& decl = utl::down_cast<ast::VariableDeclaration&>(*node.astNode);
    /// Analyze the type
    SC_ASSERT(decl.typeExpr, "Should be caught in gather stage");
    TypeID const typeID = analyzeTypeExpression(*decl.typeExpr);
    addTypeDependency(node, typeID);
    /// For now we are not declaring variables to the symbol table during prepass, unlike functions and structs which are being declared.
    /// We may want to change this behaviour for the sake of consistency.
}

TypeID Context::analyzeTypeExpression(ast::Expression& expr) {
    auto const typeExprResult = analyzeExpression(expr);
    if (!typeExprResult) {
        return TypeID::Invalid;
    }
    if (typeExprResult.category() != ast::EntityCategory::Type) {
        iss.push(BadSymbolReference(expr, expr.category, ast::EntityCategory::Type));
        return TypeID::Invalid;
    }
    auto const& type = sym.getObjectType(typeExprResult.typeID());
    return type.symbolID();
}

ExpressionAnalysisResult Context::analyzeExpression(ast::Expression& expr) {
    return sema::analyzeExpression(expr, sym, &iss);
}

void Context::addTypeDependency(DependencyGraphNode& node, TypeID typeID) const {
    if (!typeID) {
        return;
    }
    /// This should be safe since we only get a valid type id here if that type is declared.
    auto const& objType = sym.getObjectType(typeID);
    if (objType.isComplete()) {
        /// We don't need to handle this dependency because objType is already instantiated.
        /// This is usually the case for builtin types.
        return;
    }
    auto itr = graphIndices.find(typeID);
    SC_ASSERT(itr != graphIndices.end(), "Must exist");
    auto const [id, index] = *itr;
    node.dependencies.push_back(utl::narrow_cast<u16>(index));
}
