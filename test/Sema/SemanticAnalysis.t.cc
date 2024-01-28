#include <array>

#include <catch2/catch_test_macros.hpp>
#include <range/v3/algorithm.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SemaUtil.h"
#include "Sema/SimpleAnalzyer.h"
#include "Util/IssueHelper.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace sema;
using namespace ast;
using enum ValueCategory;
using test::find;
using test::lookup;

TEST_CASE("Registration in SymbolTable", "[sema]") {
    auto const text = R"(
fn mul(a: int, b: int, c: double) -> int {
	let result = a;
	return result;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* mul = lookup<Function>(sym, "mul");
    CHECK(mul->returnType() == sym.S64());
    REQUIRE(mul->argumentCount() == 3);
    CHECK(mul->argumentType(0) == sym.S64());
    CHECK(mul->argumentType(1) == sym.S64());
    CHECK(mul->argumentType(2) == sym.F64());
    auto* a = find<Variable>(mul, "a");
    CHECK(a->type() == sym.S64());
    auto* b = find<Variable>(mul, "b");
    CHECK(b->type() == sym.S64());
    auto const c = find<Variable>(mul, "c");
    CHECK(c->type() == sym.F64());
    auto* result = find<Variable>(mul, "result");
    CHECK(result->type() == sym.S64());
}

TEST_CASE("Decoration of the AST", "[sema]") {
    auto const text = R"(
fn mul(a: int, b: int, c: double, d: byte) -> int {
	let result = a;
	{ // declaration of variable of the same name in a nested scope
		var result: &str = "some string";
	}
	// declaration of float variable
	let y = 39;
	let z = 0x39E;
	let x = 1.2;
	return result;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    auto* fn = file->statement<FunctionDefinition>(0);
    CHECK(fn->returnType() == sym.S64());
    CHECK(fn->parameter(0)->type() == sym.S64());
    CHECK(fn->parameter(1)->type() == sym.S64());
    CHECK(fn->parameter(2)->type() == sym.F64());
    CHECK(fn->parameter(3)->type() == sym.Byte());
    auto* varDecl = fn->body()->statement<VariableDeclaration>(0);
    CHECK(varDecl->type() == sym.S64());
    CHECK(varDecl->initExpr()->type().get() == sym.S64());
    auto* nestedScope = fn->body()->statement<CompoundStatement>(1);
    auto* nestedVarDecl = nestedScope->statement<VariableDeclaration>(0);
    auto* nestedvarDeclInit = cast<Literal*>(nestedVarDecl->initExpr());
    CHECK(nestedvarDeclInit->type() == QualType::Const(sym.Str()));
    CHECK(nestedvarDeclInit->valueCategory() == LValue);
    auto* xDecl = fn->body()->statement<VariableDeclaration>(2);
    CHECK(xDecl->type() == sym.S64());
    auto* intLit = cast<ast::Literal*>(xDecl->initExpr());
    CHECK(intLit->value<APInt>() == 39);
    auto* zDecl = fn->body()->statement<VariableDeclaration>(3);
    CHECK(zDecl->type() == sym.S64());
    auto* intHexLit = cast<ast::Literal*>(zDecl->initExpr());
    CHECK(intHexLit->value<APInt>() == 0x39E);
    auto* yDecl = fn->body()->statement<VariableDeclaration>(4);
    CHECK(yDecl->type() == sym.F64());
    auto* floatLit = cast<ast::Literal*>(yDecl->initExpr());
    CHECK(floatLit->value<APFloat>().to<f64>() == 1.2);
    auto* ret = fn->body()->statement<ReturnStatement>(5);
    CHECK(ret->expression()->type().get() == sym.S64());
}

TEST_CASE("Decoration of the AST with function call expression", "[sema]") {
    auto const text = R"(
fn caller() -> float {
	let result = callee(1.0, 0, true);
	return result;
}
fn callee(a: float, b: int, c: bool) -> float { return 0.0; }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file);
    auto* calleeDecl = file->statement<FunctionDefinition>(1);
    REQUIRE(calleeDecl);
    CHECK(calleeDecl->returnType() == sym.F32());
    CHECK(calleeDecl->parameter(0)->type() == sym.F32());
    CHECK(calleeDecl->parameter(1)->type() == sym.S64());
    CHECK(calleeDecl->parameter(2)->type() == sym.Bool());
    auto* caller = file->statement<FunctionDefinition>(0);
    auto* resultDecl = caller->body()->statement<VariableDeclaration>(0);
    CHECK(resultDecl->initExpr()->type().get() == sym.F32());
    auto* fnCallExpr = cast<ast::FunctionCall*>(resultDecl->initExpr());
    auto* calleeFn = lookup<Function>(sym, "callee");
    CHECK(fnCallExpr->function() == calleeFn);
}

TEST_CASE("Decoration of the AST with struct definition", "[sema]") {
    auto const text = R"(
struct X {
	var i: float;
	var j: int = 0;
	var b1: bool = true;
	var b2: bool = true;
	fn f(x: int, y: int) -> byte {}
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    auto* xDef = file->statement<StructDefinition>(0);
    CHECK(xDef->name() == "X");
    auto* iDecl = xDef->body()->statement<VariableDeclaration>(0);
    CHECK(iDecl->name() == "i");
    CHECK(iDecl->type() == sym.F32());
    auto* jDecl = xDef->body()->statement<VariableDeclaration>(1);
    CHECK(jDecl->name() == "j");
    CHECK(jDecl->type() == sym.S64());
    auto* b2Decl = xDef->body()->statement<VariableDeclaration>(3);
    CHECK(b2Decl->name() == "b2");
    CHECK(b2Decl->type() == sym.Bool());
    auto* fDef = xDef->body()->statement<FunctionDefinition>(4);
    CHECK(fDef->name() == "f");
    CHECK(fDef->returnType() == sym.Byte());
}

TEST_CASE("Member access into undeclared struct", "[sema]") {
    auto const text = R"(
fn f(x: X) -> int { return x.data; }
struct X { var data: int; }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
}

TEST_CASE("Type reference access into undeclared struct", "[sema]") {
    auto const text = R"(
fn f() {
	let y: X.Y;
}
struct X { struct Y {} }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    auto* f = file->statement<FunctionDefinition>(0);
    auto* y = f->body()->statement<VariableDeclaration>(0);
    auto YType = y->type();
    CHECK(YType->name() == "Y");
    CHECK(YType->parent()->name() == "X");
    {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f() -> X.Y {}
struct X { struct Y {} }
)");
        REQUIRE(iss.empty());
    }
    {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f(y: X.Y) {}
struct X { struct Y {} }
)");
        REQUIRE(iss.empty());
    }
    {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f(x: X, y: X.Y) -> X.Y.Z {}
struct X { struct Y { struct Z{} } }
)");
        REQUIRE(iss.empty());
    }
}

TEST_CASE("Member access into rvalue", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn main() -> int { return f().data; }
fn f() -> X {
	var x: X;
	return x;
}
struct X { var data: int = 0; }
)");
    REQUIRE(iss.empty());
}

TEST_CASE("Explicit type reference to member of same scope", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
	fn f() { let y: X.Y.Z; }
	struct Y { struct Z {} }
})");
    REQUIRE(iss.empty());
}

TEST_CASE("Nested member access into undeclared struct", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f(x: X) -> int {
	let result = x.y.data;
	return result;
}
struct Y {
	var data: int;
}
struct X {
	var y: Y;
})");
    REQUIRE(iss.empty());
}

TEST_CASE("Operators on int", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn main() {
	var i = 0;
	let j = 1;
	i  =  4; i  =  j;
	i +=  4; i +=  j; i = i  + 4; i = i  + j;
	i -=  4; i -=  j; i = i  - 4; i = i  - j;
	i *=  4; i *=  j; i = i  * 4; i = i  * j;
	i /=  4; i /=  j; i = i  / 4; i = i  / j;
	i %=  4; i %=  j; i = i  % 4; i = i  % j;
	i <<= 4; i <<= j; i = i << 4; i = i << j;
	i >>= 4; i >>= j; i = i >> 4; i = i >> j;
	i &=  4; i &=  j; i = i  & 4; i = i  & j;
	i ^=  4; i ^=  j; i = i  ^ 4; i = i  ^ j;
	i |=  4; i |=  j; i = i  | 4; i = i  | j;
})");
    REQUIRE(iss.empty());
}

TEST_CASE("Operators on float", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn main() {
	var i = 0.0;
	let j = 1.0;
	i  =  4.0; i  =  j;
	i +=  4.0; i +=  j; i = i  + 4.0; i = i  + j;
	i -=  4.0; i -=  j; i = i  - 4.0; i = i  - j;
	i *=  4.0; i *=  j; i = i  * 4.0; i = i  * j;
	i /=  4.0; i /=  j; i = i  / 4.0; i = i  / j;
})");
    REQUIRE(iss.empty());
}

TEST_CASE("Possible ambiguity with later declared local struct", "[sema]") {
    auto const text = R"(
struct Y {}
struct X {
	fn f(y: Y) {}
	struct Y{}
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* x = lookup<Scope>(sym, "X");
    sym.withScopeCurrent(x, [&] {
        auto* f = lookup<Function>(sym, "f");
        CHECK(f->argumentType(0)->parent()->name() == "X");
    });
}

TEST_CASE("Conditional operator", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn test(i: int) -> int {
	let cond = i == 0;
	let a = cond ? i : 1;
})");
    REQUIRE(iss.empty());
}

/// TODO: Move this to "Parser.t.cc" and generate appropriate error
TEST_CASE("Comma expression in variable declaration", "[parse]") {
    return;
    /// Top level comma expression is not allowed in variable declaration.
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn main(i: int) -> int {
    let a = x, y;
})");
    REQUIRE(iss.empty());
}

TEST_CASE("Sizeof structs with reference and array members", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    var r: *s8;
}
struct Y {
    var r: *[s8];
}
struct Z {
    var r: [s8, 7];
}
struct W {
    var r: [s8, 7];
    var n: s64;
}
struct S {
    var arr: [Y, 2];
    struct Y { var n: int; }
}
)");
    REQUIRE(iss.empty());
    auto* x = lookup<StructType>(sym, "X");
    CHECK(x->size() == 8);
    CHECK(x->align() == 8);
    auto* y = lookup<StructType>(sym, "Y");
    CHECK(y->size() == 16);
    CHECK(y->align() == 8);
    auto* z = lookup<StructType>(sym, "Z");
    CHECK(z->size() == 7);
    CHECK(z->align() == 1);
    auto* w = lookup<StructType>(sym, "W");
    CHECK(w->size() == 16);
    CHECK(w->align() == 8);
    auto* s = lookup<StructType>(sym, "S");
    CHECK(s->size() == 16);
    CHECK(s->align() == 8);
}

TEST_CASE("Return type deduction", "[sema]") {
    SECTION("Successful") {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f(cond: bool) {
    if cond {
        return 1;
    }
    return 0;
})");
        REQUIRE(iss.empty());
        auto* f = lookup<Function>(sym, "f");
        CHECK(f->returnType() == sym.S64());
    }
    SECTION("Successful void") {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn f(cond: bool) {
    if cond {
        return;
    }
    return;
})");
        REQUIRE(iss.empty());
        auto* f = lookup<Function>(sym, "f");
        CHECK(f->returnType() == sym.Void());
    }
    SECTION("Conflicting") {
        auto const issues = test::getSemaIssues(R"(
/* 2 */ fn f(cond: bool) {
/* 3 */     return 1;
/* 4 */     return;
})");
        CHECK(!issues.empty());
        auto* issue = issues.findOnLine<BadReturnTypeDeduction>(4);
        REQUIRE(issue);
        auto* file =
            cast<ast::TranslationUnit*>(issues.ast.get())->sourceFile(0);
        auto* f = file->statement<ast::FunctionDefinition>(0);
        auto* ret1 = f->body()->statement<ast::ReturnStatement>(0);
        auto* ret2 = f->body()->statement<ast::ReturnStatement>(1);
        CHECK(issue->statement() == ret2);
        CHECK(issue->conflicting() == ret1);
    }
}

TEST_CASE("Copy value with function new but no SLFs", "[sema][regression]") {
    /// Here we just test successful analysis
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
/// Has constructors but no special lifetime functions
struct X {
    fn new(&mut this) {}
    fn new(&mut this, n: int) {}
}
fn byValue(x: X) {}
fn main() {
    let x = X();
    byValue(x);
})");
    CHECK(iss.empty());
}

TEST_CASE("This parameter by value", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    fn byValue(mut this) {
        this.value = 2;
        return this.value;
    }
    var value: int;
}
fn main() {
    var x = X(1);
    x.byValue();
})");
    CHECK(iss.empty());
}

TEST_CASE("This parameter by constant value", "[sema]") {
    auto iss = test::getSemaIssues(R"(
struct X {
    fn byValue(this) {
        this.value = 2;
    }
    var value: int;
}
fn main() {
    var x = X(1);
    x.byValue();
})");
    CHECK(iss.findOnLine<BadExpr>(4, BadExpr::AssignExprImmutableLHS));
}

TEST_CASE("Import statement", "[sema][lib]") {
    return;
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
import testlib;
)");
    REQUIRE(iss.empty());
}

TEST_CASE("Import missing library", "[sema][lib]") {
    auto iss = test::getSemaIssues(R"(
import does_not_exist;
)");
    CHECK(iss.findOnLine<BadImport>(2));
}

TEST_CASE("Unknown linkage", "[sema][lib]") {
    auto iss = test::getSemaIssues(R"(
extern "D" fn foo() -> void;
)");
    CHECK(iss.findOnLine<BadFuncDef>(2, BadFuncDef::UnknownLinkage));
}

static void compileTestlib() {
    test::compileLibrary("libs/testlib",
                         "libs",
                         R"(
public fn foo() { return 42; }
public fn bar() { return 42; }
)");
}

TEST_CASE("Import same lib in multiple scopes", "[lib][nativelib]") {
    compileTestlib();
    auto iss = test::getSemaIssues(R"(
fn testlib() {} // Clobber name 'testlib' here
fn test1() {
    import testlib;
    testlib.foo();
}
fn test2() {
    import testlib;
    testlib.foo();
})",
                                   { .librarySearchPaths = { "libs" } });
    CHECK(iss.empty());
}

TEST_CASE("Import same lib in one scope and use in other", "[lib][nativelib]") {
    compileTestlib();
    auto iss = test::getSemaIssues(R"(
fn testlib() {} // Clobber name 'testlib' here
fn test1() {
    import testlib;
    testlib.foo();
}
fn test2() {
    import testlib;
    testlib.foo();
})",
                                   { .librarySearchPaths = { "libs" } });
    CHECK(iss.empty());
}

TEST_CASE("Use nested library name", "[lib][nativelib]") {
    compileTestlib();
    auto iss = test::getSemaIssues(R"(
fn test() {
    use testlib.foo;
    foo();
})",
                                   { .librarySearchPaths = { "libs" } });
    CHECK(iss.empty());
}

TEST_CASE("Access control deduction", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    fn f() {}
}
public struct Y {
    fn f() {}
}
internal struct Z {
    fn f() {}
}
private struct W {
    fn f() {}
})");
    auto* file = (sym.globalScope().children() | Filter<FileScope>).front();
    auto* X = find<StructType>(file, "X");
    CHECK(X->isInternal());
    CHECK(find(X, "f")->isInternal());

    auto* Y = find<StructType>(file, "Y");
    CHECK(Y->isPublic());
    CHECK(find(Y, "f")->isPublic());

    auto* Z = find<StructType>(file, "Z");
    CHECK(Z->isInternal());
    CHECK(find(Z, "f")->isInternal());

    auto* W = find<StructType>(file, "W");
    CHECK(W->isPrivate());
    CHECK(find(W, "f")->isPrivate());
}

TEST_CASE("Access control errors", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ internal struct X { public fn f() {} }
/*  3 */ private struct Y { internal fn f() {} }
/*  4 */ struct InternalType {}
/*  5 */ public struct PublicType {
/*  6 */     var member: InternalType;
/*  7 */ }
/*  8 */ public fn publicFunction(x: InternalType) {}
/*  9 */ public fn publicFunction() { return InternalType(); }
/* 10 */ struct Foo { private var p: int; }
/* 11 */ fn accessFoo_p(foo: Foo) { foo.p; }
/* 12 */ fn accessLocalVarInNestedScope(x: int) { { x; } }
)");
    using enum BadAccessControl::Reason;
    CHECK(iss.findOnLine<BadAccessControl>(2, TooWeakForParent));
    CHECK(iss.findOnLine<BadAccessControl>(3, TooWeakForParent));
    CHECK(iss.findOnLine<BadAccessControl>(6, TooWeakForType));
    CHECK(iss.findOnLine<BadAccessControl>(8, TooWeakForType));
    CHECK(iss.findOnLine<BadAccessControl>(9, TooWeakForType));
    CHECK(iss.findOnLine<BadExpr>(11, BadExpr::AccessDenied));
    CHECK(iss.noneOnLine(12));
}

TEST_CASE("Using private variables in member functions", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ struct X {
/*  3 */     fn new(&mut this) {
/*  4 */         this.value = 7;
/*  5 */     }
/*  6 */     fn getValue(&this) -> int {
/*  7 */         return this.value;
/*  8 */     }
/*  9 */     private var value: int;
/* 10 */ }
/* 11 */ fn test(x: X) {
/* 12 */     x.getValue();
/* 13 */     x.value;
/* 14 */ }
)");
    CHECK(iss.noneOnLine(4));
    CHECK(iss.noneOnLine(7));
    CHECK(iss.noneOnLine(12));
    CHECK(iss.findOnLine<BadExpr>(13, BadExpr::AccessDenied));
}

TEST_CASE("Array lifetimes properly analyzed", "[sema]") {
    /// In this sample program the type `[X, 1]` would not have its lifetime
    /// analyzed because it is instantiated before `X` has its lifetime analyzed
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {}
fn foo(x: [X, 1]) {}
fn bar() { let x = [X()]; }
)");
    CHECK(iss.empty());
}

TEST_CASE("By value member function call with nontrivial copy constructor",
          "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
public fn test(x: &X) {
    x.foo();
}
public struct X {
    fn new(&mut this, x: &X) {}
    fn delete(&mut this) {}
    fn foo(this) {}
})");
    REQUIRE(iss.empty());
    auto* testFn = cast<TranslationUnit const*>(ast.get())
                       ->sourceFile(0)
                       ->statement<FunctionDefinition>(0);
    auto* call = testFn->body()
                     ->statement<ExpressionStatement>(0)
                     ->expression<FunctionCall>();
    CHECK(call->callee<Identifier>()->value() == "foo");
    auto* construct = call->argument<NontrivConstructExpr>(0);
    CHECK(construct->argument<Identifier>(0)->value() == "x");
}

TEST_CASE("Reachability", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ fn foo() -> int {
/*  3 */     if true {
/*  4 */         return 1;
/*  5 */         foo();
/*  6 */     }
/*  7 */     else {
/*  8 */         return 3;
/*  9 */         return 5;
/* 10 */     }
/* 11 */ })");
    CHECK(iss.findOnLine<GenericBadStmt>(5, GenericBadStmt::Unreachable));
    CHECK(iss.findOnLine<GenericBadStmt>(8, GenericBadStmt::Unreachable));
    CHECK(iss.noneOnLine(9));
}
