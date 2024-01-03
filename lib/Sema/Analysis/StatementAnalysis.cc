#include "Sema/Analysis/StatementAnalysis.h"

#include <utl/scope_guard.hpp>
#include <utl/stack.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/DtorStack.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

static void gatherParentDestructorsImpl(ast::Statement& stmt, auto condition) {
    for (auto* parentScope = cast<ast::Statement*>(stmt.parent());
         condition(parentScope);
         parentScope = dyncast<ast::Statement*>(parentScope->parent()))
    {
        for (auto dtorCall: parentScope->dtorStack() | ranges::views::reverse) {
            stmt.pushDtor(dtorCall);
        }
    }
}

static void gatherParentDestructors(ast::ReturnStatement& stmt) {
    gatherParentDestructorsImpl(stmt, [](auto* parent) {
        return !isa<ast::FunctionDefinition>(parent);
    });
}

static void gatherParentDestructors(ast::JumpStatement& stmt) {
    gatherParentDestructorsImpl(stmt, [](auto* parent) {
        return !isa<ast::LoopStatement>(parent);
    });
}

namespace {

struct StmtContext {
    StmtContext(sema::AnalysisContext& ctx):
        ctx(ctx), sym(ctx.symbolTable()), iss(ctx.issueHandler()) {}

    void analyze(ast::ASTNode&);

    void analyzeImpl(ast::ImportStatement&);
    void importUnscopedSymbols(ast::ImportStatement& stmt);
    bool validateNativeLibImport(ast::ImportStatement& stmt,
                                 ast::Identifier& ID);
    bool validateForeignLibImport(ast::ImportStatement& stmt,
                                  ast::Literal& lit);
    void analyzeImpl(ast::FunctionDefinition&);
    void analyzeMainFunction();
    void analyzeImpl(ast::ParameterDeclaration&);
    void analyzeImpl(ast::ThisParameter&);
    void analyzeImpl(ast::StructDefinition&);
    void analyzeImpl(ast::CompoundStatement&);
    void analyzeImpl(ast::VariableDeclaration&);
    void analyzeImpl(ast::ExpressionStatement&);
    void analyzeImpl(ast::ReturnStatement&);
    void analyzeImpl(ast::IfStatement&);
    void analyzeImpl(ast::LoopStatement&);
    void analyzeImpl(ast::JumpStatement&);
    void analyzeImpl(ast::EmptyStatement&) {}
    void analyzeImpl(ast::ASTNode&) { SC_UNREACHABLE(); }

    ast::Expression* analyzeExpr(ast::Expression* expr, DtorStack& dtorStack) {
        return sema::analyzeExpression(expr, dtorStack, ctx);
    }

    ast::Expression* analyzeValue(ast::Expression* expr, DtorStack& dtorStack) {
        return sema::analyzeValueExpr(expr, dtorStack, ctx);
    }

    Type const* analyzeType(ast::Expression* expr) {
        return sema::analyzeTypeExpr(expr, ctx);
    }

    /// Used by return statement case to add a type to return type deduction
    void deduceReturnTypeTo(ast::ReturnStatement const* stmt,
                            sema::Type const* type);

    /// Called by function definition case after analyzing the body
    void setDeducedReturnType();

    /// \Returns `true` if the current function returns by reference. In that
    /// case we don't pop destructor for our return value
    bool returnsRef() const {
        /// For now! If we add slim ref qualifiers with type deduction this
        /// needs to change
        if (!currentFunction->returnType()) {
            return false;
        }
        return isa<ReferenceType>(currentFunction->returnType());
    }

    sema::AnalysisContext& ctx;
    SymbolTable& sym;
    IssueHandler& iss;
    ast::FunctionDefinition* currentFunction;
    sema::Function* semaFn;
    /// Only needed if return type is not specified
    sema::Type const* deducedRetTy = nullptr;
    ast::ReturnStatement const* lastReturn = nullptr;
};

} // namespace

void sema::analyzeStatement(AnalysisContext& ctx, ast::Statement* stmt) {
    StmtContext stmtCtx(ctx);
    stmtCtx.analyze(*stmt);
}

void StmtContext::analyze(ast::ASTNode& node) {
    visit(node, [this](auto& node) { this->analyzeImpl(node); });
}

///
static ast::Expression* getLibName(ast::Expression& importExpr) {
    // clang-format off
    return SC_MATCH (importExpr) {
        [&](ast::Literal& lit) -> ast::Expression* {
            return &lit;
        },
        [&](ast::Identifier& ID) -> ast::Expression* {
            return &ID;
        },
        [&](ast::MemberAccess& MA) -> ast::Expression* {
            ast::Expression* expr = &MA;
            while (true) {
                auto* ma = dyncast<ast::MemberAccess*>(expr);
                if (!ma) {
                    return dyncast<ast::Identifier*>(expr);
                }
                expr = ma->accessed();
            }
        },
        [&](ast::Expression&) -> ast::Expression* {
            return nullptr;
        }
    }; // clang-format on
}

void StmtContext::analyzeImpl(ast::ImportStatement& stmt) {
    if (!stmt.libExpr()) {
        return;
    }
    /// We first determine which part of the library expression denotes the name
    /// of the library
    auto* libname = getLibName(*stmt.libExpr());
    if (!libname) {
        ctx.issue<BadImport>(stmt.libExpr(), BadImport::InvalidExpression);
        return;
    }
    /// Then we make the library available in the current scope or import the
    /// foreign library
    // clang-format off
    auto* lib = SC_MATCH (*libname) {
        [&](ast::Literal& lit) -> Library* {
            if (!validateForeignLibImport(stmt, lit)) {
                return nullptr;
            }
            return sym.importForeignLib(lit);
        },
        [&](ast::Identifier& ID) -> Library* {
            if (!validateNativeLibImport(stmt, ID)) {
                return nullptr;
            }
            return sym.makeNativeLibAvailable(ID);
        },
        [&](ast::Expression&) -> Library* {
            SC_UNREACHABLE();
        }
    }; // clang-format on
    if (!lib) {
        return;
    }
    stmt.decorateStmt(lib);
    /// Then once the library name is available in the current scope we can
    /// analyze the import expression
    if (!analyzeExpr(stmt.libExpr(), stmt.dtorStack())) {
        return;
    }
    if (stmt.importKind() == ImportKind::Unscoped) {
        importUnscopedSymbols(stmt);
    }
}

void StmtContext::importUnscopedSymbols(ast::ImportStatement& stmt) {
    SC_EXPECT(stmt.importKind() == ImportKind::Unscoped);
    if (auto* ID = dyncast<ast::Identifier*>(stmt.libExpr())) {
        auto& lib = cast<NativeLibrary&>(*stmt.library());
        for (auto* entity: lib.entities()) {
            sym.declareAlias(*entity, ID);
        }
    }
    else {
        auto* expr = stmt.libExpr();
        SC_ASSERT(isa<ast::MemberAccess>(expr),
                  "Other cases should produce issues above");
        SC_ASSERT(expr->entity(), "We should not be here if analysis failed");
        sym.declareAlias(*expr->entity(), expr);
    }
}

bool StmtContext::validateNativeLibImport(ast::ImportStatement& stmt,
                                          ast::Identifier&) {
    bool success = true;
    if (stmt.importKind() == ImportKind::Scoped &&
        !isa<ast::Identifier>(stmt.libExpr()))
    {
        ctx.issue<BadImport>(&stmt, BadImport::InvalidExpression);
        success = false;
    }
    return success;
}

bool StmtContext::validateForeignLibImport(ast::ImportStatement& stmt,
                                           ast::Literal& lit) {
    bool success = true;
    if (lit.kind() != ast::LiteralKind::String) {
        ctx.issue<BadImport>(&stmt, BadImport::InvalidExpression);
        success = false;
    }
    if (stmt.importKind() != ImportKind::Scoped) {
        ctx.issue<BadImport>(&stmt, BadImport::UnscopedForeignLibImport);
        success = false;
    }
    if (sym.currentScope().kind() != ScopeKind::Global) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        success = false;
    }
    return success;
}

void StmtContext::analyzeImpl(ast::FunctionDefinition& def) {
    if (auto const sk = sym.currentScope().kind(); sk != ScopeKind::Global &&
                                                   sk != ScopeKind::Namespace &&
                                                   sk != ScopeKind::Type)
    {
        /// Function defintion is only allowed in the global scope, at namespace
        /// scope and structure scope.
        ctx.issue<GenericBadStmt>(&def, GenericBadStmt::InvalidScope);
        sym.declarePoison(def.nameIdentifier(), EntityCategory::Value);
        return;
    }
    currentFunction = &def;
    semaFn = def.function();
    /// Here the AST node is partially decorated: `entity()` is already set by
    /// `gatherNames()` phase, now we complete the decoration.
    if (ctx.isAnalyzed(semaFn) || ctx.isAnalyzing(semaFn)) {
        /// We don't emit errors here if the function is currently analyzing
        /// because the error should appear at the callsite
        return;
    }
    ctx.beginAnalyzing(semaFn);
    utl::scope_guard guard([&] { ctx.endAnalyzing(semaFn); });
    def.decorateFunction(semaFn, semaFn->returnType());
    semaFn->setBinaryVisibility(def.binaryVisibility());
    sym.withScopePushed(semaFn, [&] {
        for (auto* param: def.parameters()) {
            analyze(*param);
        }
    });
    if (auto linkage = def.externalLinkage();
        !linkage.empty() && linkage != "C")
    {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::UnknownLinkage);
    }
    if (def.body()) {
        def.body()->decorateScope(semaFn);
        analyze(*def.body());
        setDeducedReturnType();
    }
    else {
        if (def.externalLinkage().empty()) {
            ctx.issue<BadFuncDef>(&def, BadFuncDef::FunctionMustHaveBody);
        }
        else if (!semaFn->returnType()) {
            ctx.issue<BadFuncDef>(
                &def,
                BadFuncDef::FunctionDeclarationHasNoReturnType);
        }
    }
    /// We perform the extra checks on main in the end because here we have
    /// deduced the return type
    if (semaFn->name() == "main") {
        analyzeMainFunction();
    }
}

/// \Returns `true` if \p sig is either `() -> "any"` or `(&[*str]) -> "any"`
static bool argumentsAreValidForMain(std::span<Type const* const> types,
                                     SymbolTable& sym) {
    switch (types.size()) {
    case 0:
        return true;
    case 1: {
        auto* type = types.front();
        auto* constStrPtr = sym.pointer(QualType::Const(sym.Str()));
        auto* array = sym.arrayType(constStrPtr);
        auto* ref = sym.reference(QualType::Const(array));
        return type == ref;
    }
    default:
        return false;
    }
}

/// Here we perform all checks and transforms on `main` that make it special
void StmtContext::analyzeMainFunction() {
    /// main is always binary visible
    semaFn->setBinaryVisibility(BinaryVisibility::Export);
    /// We might require main to return int at some point, but right now there
    /// are many test cases where main returns bool or double
    auto* retType = semaFn->returnType();
    if (!retType->hasTrivialLifetime()) {
        ctx.issue<BadFuncDef>(currentFunction,
                              BadFuncDef::MainMustReturnTrivial);
    }
    /// Only certain argument types are valid for main
    if (!argumentsAreValidForMain(semaFn->argumentTypes(), sym)) {
        ctx.issue<BadFuncDef>(currentFunction,
                              BadFuncDef::MainInvalidArguments);
    }
}

void StmtContext::analyzeImpl(ast::ParameterDeclaration& paramDecl) {
    Type const* declaredType = semaFn->argumentType(paramDecl.index());
    if (!declaredType) {
        sym.declarePoison(paramDecl.nameIdentifier(), EntityCategory::Value);
        return;
    }
    auto* param =
        sym.defineVariable(&paramDecl, declaredType, paramDecl.mutability());
    if (param) {
        paramDecl.decorateVarDecl(param);
    }
}

void StmtContext::analyzeImpl(ast::ThisParameter& thisParam) {
    auto* parentType = dyncast<ObjectType*>(semaFn->parent());
    if (!parentType) {
        return;
    }
    /// We already check the position during instantiation
    auto* param = [&] {
        Type const* type = parentType;
        auto mut = thisParam.mutability();
        if (thisParam.isReference()) {
            type = sym.reference({ parentType, thisParam.mutability() });
            mut = Mutability::Const;
        }
        return sym.addProperty(PropertyKind::This, type, mut, LValue);
    }();
    if (param) {
        semaFn->setIsMember();
        thisParam.decorateVarDecl(param);
    }
}

void StmtContext::analyzeImpl(ast::StructDefinition& def) {
    /// Function defintion is only allowed in the global scope, at namespace
    /// scope and structure scope.
    sym.declarePoison(def.nameIdentifier(), EntityCategory::Type);
    ctx.issue<GenericBadStmt>(&def, GenericBadStmt::InvalidScope);
}

void StmtContext::analyzeImpl(ast::CompoundStatement& block) {
    if (!block.isDecorated()) {
        block.decorateScope(sym.addAnonymousScope());
    }
    sym.withScopePushed(block.scope(), [&] {
        /// We make a copy why?
        for (auto* statement: block.statements() | ToSmallVector<>) {
            analyze(*statement);
        }
    });
}

void StmtContext::analyzeImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(!varDecl.isDecorated(),
              "We should not have handled local variables in prepass.");
    /// We need at least one of init expression and type specifier
    if (!varDecl.initExpr() && !varDecl.typeExpr()) {
        sym.declarePoison(varDecl.nameIdentifier(), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::CantInferType);
        return;
    }
    auto* initExpr = analyzeValue(varDecl.initExpr(), varDecl.dtorStack());
    auto declType = analyzeType(varDecl.typeExpr());
    auto initType = initExpr ? initExpr->type().get() : nullptr;
    auto type = declType ? declType : initType;
    /// We cannot deduce the type of the variable
    if (!type) {
        sym.declarePoison(varDecl.nameIdentifier(), EntityCategory::Value);
        return;
    }
    /// The type must be complete, that means no `void` and no dynamic arrays
    if (!type->isComplete()) {
        sym.declarePoison(varDecl.nameIdentifier(), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl,
                              BadVarDecl::IncompleteType,
                              type,
                              initExpr);
        return;
    }
    /// Reference variables must be initalized explicitly
    if (isa<ReferenceType>(type) && !initExpr) {
        sym.declarePoison(varDecl.nameIdentifier(), EntityCategory::Value);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::ExpectedRefInit);
        return;
    }
    /// If the symbol table complains we also return early
    auto* variable = sym.defineVariable(&varDecl, type, varDecl.mutability());
    if (!variable) {
        return;
    }
    varDecl.decorateVarDecl(variable);
    /// If we have an init expression we convert it to the type of the variable.
    /// If the type is derived from the init expression then this is a no-op
    if (varDecl.initExpr()) {
        /// Test `initExpr` because the variable may have an invalid init
        /// expression, in this case `varDecl.initExpr()` is not null but
        /// `initExpr` is.
        if (initExpr) {
            initExpr = convert(Implicit,
                               initExpr,
                               variable->getQualType(),
                               refToLValue(type),
                               varDecl.dtorStack(),
                               ctx);
        }
    }
    /// Otherwise we construct an object of the declared type without arguments
    else {
        auto* objType = cast<ObjectType const*>(type);
        auto constructExpr =
            allocate<ast::ConstructExpr>(objType, varDecl.sourceRange());
        initExpr = varDecl.setInitExpr(std::move(constructExpr));
        initExpr = analyzeValue(initExpr, varDecl.dtorStack());
    }
    /// If our variable is of object type, we pop the last destructor _in the
    /// stack of this declaration_ because it corresponds to the object whose
    /// lifetime this variable shall extend. Then we push the destructor to the
    /// stack of the parent statement.
    if (!isa<ReferenceType>(varDecl.type())) {
        popTopLevelDtor(initExpr, varDecl.dtorStack());
        cast<ast::Statement*>(varDecl.parent())->pushDtor(variable);
    }
    /// Propagate constant value
    if (variable->isConst() && initExpr) {
        variable->setConstantValue(clone(initExpr->constantValue()));
    }
}

void StmtContext::analyzeImpl(ast::ExpressionStatement& es) {
    SC_EXPECT(sym.currentScope().kind() == ScopeKind::Function);
    analyzeValue(es.expression(), es.dtorStack());
}

void StmtContext::analyzeImpl(ast::ReturnStatement& rs) {
    SC_EXPECT(sym.currentScope().kind() == ScopeKind::Function);
    /// We gather parent destructors here because `analyzeValue()` may add more
    /// constructors and the parent destructors must be lower in the stack
    gatherParentDestructors(rs);
    Type const* returnType = currentFunction->returnType();
    /// "Naked" `return;` case
    if (!rs.expression()) {
        if (!returnType) {
            deduceReturnTypeTo(&rs, sym.Void());
        }
        else if (!isa<VoidType>(returnType)) {
            ctx.issue<BadReturnStmt>(&rs,
                                     BadReturnStmt::NonVoidMustReturnValue);
        }
        /// Else we return `void` as expected
        return;
    }
    /// We return an expression
    if (!analyzeValue(rs.expression(), rs.dtorStack())) {
        return;
    }
    if (isa<VoidType>(returnType)) {
        ctx.issue<BadReturnStmt>(&rs, BadReturnStmt::VoidMustNotReturnValue);
        return;
    }
    if (!returnType) {
        returnType = rs.expression()->type().get();
        deduceReturnTypeTo(&rs, returnType);
    }
    convert(Implicit,
            rs.expression(),
            getQualType(returnType),
            refToLValue(returnType),
            rs.dtorStack(),
            ctx);
    if (!returnsRef()) {
        popTopLevelDtor(rs.expression(), rs.dtorStack());
    }
}

void StmtContext::analyzeImpl(ast::IfStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        return;
    }
    if (analyzeValue(stmt.condition(), stmt.dtorStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.dtorStack(),
                ctx);
    }
    analyze(*stmt.thenBlock());
    if (stmt.elseBlock()) {
        analyze(*stmt.elseBlock());
    }
}

void StmtContext::analyzeImpl(ast::LoopStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        return;
    }
    stmt.block()->decorateScope(sym.addAnonymousScope());
    sym.pushScope(stmt.block()->scope());
    if (stmt.varDecl()) {
        analyze(*stmt.varDecl());
    }
    if (analyzeValue(stmt.condition(), stmt.conditionDtorStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.conditionDtorStack(),
                ctx);
    }
    if (stmt.increment()) {
        analyzeValue(stmt.increment(), stmt.incrementDtorStack());
    }
    sym.popScope(); /// The block will push its scope again.
    analyze(*stmt.block());
}

void StmtContext::analyzeImpl(ast::JumpStatement& stmt) {
    auto* parent = stmt.parent();
    while (true) {
        if (!parent || isa<ast::FunctionDefinition>(parent)) {
            ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
            return;
        }
        if (isa<ast::LoopStatement>(parent)) {
            break;
        }
        parent = parent->parent();
    }
    gatherParentDestructors(stmt);
}

void StmtContext::deduceReturnTypeTo(ast::ReturnStatement const* stmt,
                                     sema::Type const* type) {
    if (!deducedRetTy) {
        deducedRetTy = type;
    }
    if (deducedRetTy != type) {
        ctx.issue<BadReturnTypeDeduction>(stmt, lastReturn);
    }
    lastReturn = stmt;
}

void StmtContext::setDeducedReturnType() {
    if (semaFn->returnType()) {
        return;
    }
    if (!deducedRetTy) {
        semaFn->setDeducedReturnType(sym.Void());
        return;
    }
    if (deducedRetTy != sym.Void() && !deducedRetTy->isComplete()) {
        ctx.issue<BadPassedType>(lastReturn->expression(),
                                 BadPassedType::ReturnDeduced);
    }
    semaFn->setDeducedReturnType(deducedRetTy);
}
