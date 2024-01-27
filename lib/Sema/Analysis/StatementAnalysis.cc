#include "Sema/Analysis/StatementAnalysis.h"

#include <range/v3/algorithm.hpp>
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
#include "Sema/CleanupStack.h"
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
        for (auto dtorCall:
             parentScope->cleanupStack() | ranges::views::reverse)
        {
            stmt.pushCleanup(dtorCall);
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

    SC_NODEBUG void analyze(ast::ASTNode&);

    void analyzeImpl(ast::ImportStatement&);
    void importUnscopedSymbols(ast::ImportStatement& stmt);
    bool validateNativeLibImport(ast::ImportStatement& stmt,
                                 ast::Identifier& ID);
    bool validateForeignLibImport(ast::ImportStatement& stmt,
                                  ast::Literal& lit);
    void analyzeImpl(ast::FunctionDefinition&);
    bool validateForeignFunction(ast::FunctionDefinition&);
    void analyzeMainFunction(ast::FunctionDefinition& def);
    void analyzeNewMoveDelete(ast::FunctionDefinition& def);
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

    ast::Expression* analyzeExpr(ast::Expression* expr,
                                 CleanupStack& cleanupStack) {
        return sema::analyzeExpression(expr, cleanupStack, ctx);
    }

    ast::Expression* analyzeValue(ast::Expression* expr,
                                  CleanupStack& cleanupStack) {
        return sema::analyzeValueExpr(expr, cleanupStack, ctx);
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
    visit(node, [this](auto& node) SC_NODEBUG { this->analyzeImpl(node); });
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
    if (!analyzeExpr(stmt.libExpr(), stmt.cleanupStack())) {
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
            sym.declareAlias(*entity, ID, AccessControl::Private);
        }
    }
    else {
        auto* expr = stmt.libExpr();
        SC_ASSERT(isa<ast::MemberAccess>(expr),
                  "Other cases should produce issues above");
        SC_ASSERT(expr->entity(), "We should not be here if analysis failed");
        if (auto* OS = dyncast<OverloadSet*>(expr->entity())) {
            for (auto* F: *OS) {
                sym.declareAlias(*F, expr, AccessControl::Private);
            }
        }
        else {
            sym.declareAlias(*expr->entity(), expr, AccessControl::Private);
        }
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
        sym.declarePoison(def.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
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
    sym.withScopePushed(semaFn, [&] {
        for (auto* param: def.parameters()) {
            analyze(*param);
            /// Functions are responsible to clean up their arguments
            if (param->isDecorated() && def.body()) {
                auto* obj = cast<Object*>(param->entity());
                def.body()->cleanupStack().push(obj);
            }
        }
    });
    if (def.externalLinkage()) {
        validateForeignFunction(def);
        return;
    }
    if (!def.body()) {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::FunctionMustHaveBody);
        return;
    }
    def.body()->decorateScope(semaFn);
    analyze(*def.body());
    setDeducedReturnType();
    /// We perform the extra checks on main in the end because here we have
    /// deduced the return type
    if (semaFn->name() == "main") {
        analyzeMainFunction(def);
    }
    if (isNewMoveDelete(*semaFn)) {
        analyzeNewMoveDelete(def);
    }
}

static bool isValidTypeForFFIArg(Type const* type) {
    // clang-format off
    return SC_MATCH (*type){ [](VoidType const&) { return true; },
        [](IntType const& type) {
            return ranges::contains(std::array{ 8, 16, 32, 64 }, 
                                    type.bitwidth());
        },
        [](ByteType const&) { return true; },
        [](BoolType const&) { return true; },
        [](FloatType const&) { return true; },
        [](RawPtrType const&) { return true; },
        [](Type const&) { return false; }
    }; // clang-format on
}

static bool isValidTypeForFFIReturn(Type const* type) {
    return isValidTypeForFFIArg(type) && !isa<PointerType>(type);
}

bool StmtContext::validateForeignFunction(ast::FunctionDefinition& def) {
    SC_EXPECT(def.externalLinkage());
    Function* semaFn = def.function();
    std::string linkage = *def.externalLinkage();
    if (linkage != "C") {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::UnknownLinkage);
        return false;
    }
    bool success = true;
    if (!semaFn->returnType()) {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::NoReturnType);
        success = false;
    }
    for (auto* param: def.parameters()) {
        if (!isValidTypeForFFIArg(param->type())) {
            ctx.issue<BadVarDecl>(param, BadVarDecl::InvalidTypeForFFI);
            success = false;
        }
    }
    if (def.returnType() && !isValidTypeForFFIReturn(def.returnType())) {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::InvalidReturnTypeForFFI);
        success = false;
    }
    return success;
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
void StmtContext::analyzeMainFunction(ast::FunctionDefinition& def) {
    SC_EXPECT(semaFn == def.function());
    if (auto specifiedAccessControl = semaFn->definition()->accessControl()) {
        // TODO: Push an error here
        SC_ASSERT(*specifiedAccessControl == AccessControl::Public,
                  "Main function cannot be declared less than public");
    }
    /// main is always public
    semaFn->setAccessControl(AccessControl::Public);
    /// We might require main to return int at some point, but right now there
    /// are many test cases where main returns bool or double
    auto* retType = semaFn->returnType();
    if (!retType->hasTrivialLifetime()) {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::MainMustReturnTrivial);
    }
    /// Only certain argument types are valid for main
    if (!argumentsAreValidForMain(semaFn->argumentTypes(), sym)) {
        ctx.issue<BadFuncDef>(&def, BadFuncDef::MainInvalidArguments);
    }
}

void StmtContext::analyzeNewMoveDelete(ast::FunctionDefinition& def) {
    SC_EXPECT(semaFn == def.function());
    auto* parent = dyncast<StructType*>(semaFn->parent());
    if (!parent) {
        ctx.issue<BadSMF>(&def, BadSMF::NotInStruct, parent);
    }
    if (def.returnTypeExpr()) {
        ctx.issue<BadSMF>(&def, BadSMF::HasReturnType, parent);
    }
    Type const* mutRef = sym.reference(QualType::Mut(parent));
    if (semaFn->argumentCount() == 0) {
        ctx.issue<BadSMF>(&def, BadSMF::NoParams, parent);
    }
    else if (semaFn->argumentType(0) != mutRef) {
        ctx.issue<BadSMF>(&def, BadSMF::BadFirstParam, parent);
    }
    if (semaFn->name() == "move") {
        if (semaFn->argumentCount() != 2 || semaFn->argumentType(1) != mutRef) {
            ctx.issue<BadSMF>(&def, BadSMF::MoveSignature, parent);
        }
    }
    else if (semaFn->name() == "delete") {
        if (semaFn->argumentCount() != 1) {
            ctx.issue<BadSMF>(&def, BadSMF::DeleteSignature, parent);
        }
    }
}

void StmtContext::analyzeImpl(ast::ParameterDeclaration& paramDecl) {
    Type const* declaredType = semaFn->argumentType(paramDecl.index());
    if (!declaredType) {
        sym.declarePoison(paramDecl.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
        return;
    }
    auto* param = sym.defineVariable(&paramDecl,
                                     declaredType,
                                     paramDecl.mutability(),
                                     AccessControl::Private);
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
        return sym.addProperty(PropertyKind::This,
                               type,
                               mut,
                               LValue,
                               AccessControl::Private);
    }();
    if (param) {
        thisParam.decorateVarDecl(param);
    }
}

void StmtContext::analyzeImpl(ast::StructDefinition& def) {
    /// Function defintion is only allowed in the global scope, at namespace
    /// scope and structure scope.
    sym.declarePoison(def.nameIdentifier(),
                      EntityCategory::Type,
                      AccessControl::Private);
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

static UniquePtr<ast::ConstructBase> allocateDefaultConstruction(
    SourceRange sourceRange, ObjectType const* type) {
    auto& md = type->lifetimeMetadata();
    if (md.defaultConstructor().isTrivial()) {
        return allocate<ast::TrivDefConstructExpr>(sourceRange, type);
    }
    auto defConstr = md.defaultConstructor();
    SC_ASSERT(!defConstr.isDeleted(), "Should be caught earlier");
    using enum LifetimeOperation::Kind;
    using ArgList = utl::small_vector<UniquePtr<ast::Expression>>;
    switch (defConstr.kind()) {
    case Nontrivial:
        return allocate<ast::NontrivConstructExpr>(ArgList{},
                                                   sourceRange,
                                                   cast<StructType const*>(
                                                       type));
    case NontrivialInline:
        return allocate<ast::NontrivInlineConstructExpr>(ArgList{},
                                                         sourceRange,
                                                         type);
    default:
        SC_UNREACHABLE();
    }
}

void StmtContext::analyzeImpl(ast::VariableDeclaration& varDecl) {
    SC_ASSERT(!varDecl.isDecorated(),
              "We should not have handled local variables in prepass.");
    /// We need at least one of init expression and type specifier
    if (!varDecl.initExpr() && !varDecl.typeExpr()) {
        sym.declarePoison(varDecl.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::CantInferType);
        return;
    }
    auto* validatedInitExpr =
        analyzeValue(varDecl.initExpr(), varDecl.cleanupStack());
    auto declType = analyzeType(varDecl.typeExpr());
    auto initType = validatedInitExpr ? validatedInitExpr->type().get() :
                                        nullptr;
    auto deducedType = declType ? declType : initType;
    /// We cannot deduce the type of the variable
    if (!deducedType) {
        sym.declarePoison(varDecl.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
        return;
    }
    /// The type must be complete, that means no `void` and no dynamic arrays
    if (!deducedType->isComplete()) {
        sym.declarePoison(varDecl.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
        ctx.issue<BadVarDecl>(&varDecl,
                              BadVarDecl::IncompleteType,
                              deducedType,
                              validatedInitExpr);
        return;
    }
    /// Reference variables must be initalized explicitly
    if (isa<ReferenceType>(deducedType) && !validatedInitExpr) {
        sym.declarePoison(varDecl.nameIdentifier(),
                          EntityCategory::Value,
                          AccessControl::Private);
        ctx.issue<BadVarDecl>(&varDecl, BadVarDecl::ExpectedRefInit);
        return;
    }
    /// If the symbol table complains we also return early
    auto* variable = sym.defineVariable(&varDecl,
                                        deducedType,
                                        varDecl.mutability(),
                                        AccessControl::Private);
    if (!variable) {
        return;
    }
    varDecl.decorateVarDecl(variable);
    /// If we have an init expression we convert it to the type of the variable.
    /// If the type is derived from the init expression then this is a no-op.
    if (validatedInitExpr) {
        if (isa<BuiltinType>(deducedType)) {
            validatedInitExpr = convert(Implicit,
                                        validatedInitExpr,
                                        variable->getQualType(),
                                        RValue,
                                        varDecl.cleanupStack(),
                                        ctx);
        }
        else if (isa<ReferenceType>(deducedType)) {
            validatedInitExpr = convert(Implicit,
                                        validatedInitExpr,
                                        variable->getQualType(),
                                        LValue,
                                        varDecl.cleanupStack(),
                                        ctx);
        }
        else if (!validatedInitExpr->isRValue()) {
            validatedInitExpr = insertConstruction(validatedInitExpr,
                                                   varDecl.cleanupStack(),
                                                   ctx);
        }
    }
    /// Otherwise we construct an object of the declared type without arguments
    else {
        /// Cannot be a reference type because reference type variables require
        /// init expressions
        auto* objType = cast<ObjectType const*>(deducedType);
        auto constructExpr =
            allocateDefaultConstruction(varDecl.sourceRange(), objType);
        validatedInitExpr =
            analyzeValue(varDecl.setInitExpr(std::move(constructExpr)),
                         varDecl.cleanupStack());
    }
    /// If our variable is of object type, we pop the last destructor _in the
    /// stack of this declaration_ because it corresponds to the object whose
    /// lifetime this variable shall extend. Then we push the destructor to the
    /// stack of the parent statement.
    if (!isa<ReferenceType>(varDecl.type())) {
        popCleanup(validatedInitExpr, varDecl.cleanupStack());
        cast<ast::Statement*>(varDecl.parent())->pushCleanup(variable);
    }
    /// Propagate constant value
    if (variable->isConst() && validatedInitExpr) {
        variable->setConstantValue(clone(validatedInitExpr->constantValue()));
    }
}

void StmtContext::analyzeImpl(ast::ExpressionStatement& stmt) {
    SC_EXPECT(sym.currentScope().kind() == ScopeKind::Function);
    analyzeValue(stmt.expression(), stmt.cleanupStack());
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
    if (!analyzeValue(rs.expression(), rs.cleanupStack())) {
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
            rs.cleanupStack(),
            ctx);
    if (!returnsRef()) {
        popCleanup(rs.expression(), rs.cleanupStack());
    }
}

void StmtContext::analyzeImpl(ast::IfStatement& stmt) {
    if (sym.currentScope().kind() != ScopeKind::Function) {
        ctx.issue<GenericBadStmt>(&stmt, GenericBadStmt::InvalidScope);
        return;
    }
    if (analyzeValue(stmt.condition(), stmt.cleanupStack())) {
        convert(Implicit,
                stmt.condition(),
                sym.Bool(),
                RValue,
                stmt.cleanupStack(),
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
        sym.setFunctionType(semaFn, semaFn->argumentTypes(), sym.Void());
        return;
    }
    if (deducedRetTy != sym.Void() && !deducedRetTy->isComplete()) {
        ctx.issue<BadPassedType>(lastReturn->expression(),
                                 BadPassedType::ReturnDeduced);
    }
    sym.setFunctionType(semaFn, semaFn->argumentTypes(), deducedRetTy);
}
