#include <Catch/Catch2.hpp>

#include <array>

#include <range/v3/algorithm.hpp>

#include "AST/AST.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using namespace ast;
using enum ValueCategory;

TEST_CASE("Registration in SymbolTable", "[sema]") {
    auto const text = R"(
fn mul(a: int, b: int, c: double) -> int {
	let result = a;
	return result;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* mul = sym.lookup<OverloadSet>("mul");
    REQUIRE(mul);
    auto const* mulFn = mul->front();
    REQUIRE(mulFn);
    auto const& fnType = mulFn->signature();
    CHECK(fnType.returnType().get() == sym.S64());
    REQUIRE(fnType.argumentCount() == 3);
    CHECK(fnType.argumentType(0).get() == sym.S64());
    CHECK(fnType.argumentType(1).get() == sym.S64());
    CHECK(fnType.argumentType(2).get() == sym.F64());
    auto* a = mulFn->findEntity<Variable>("a");
    CHECK(a->type().get() == sym.S64());
    auto* b = mulFn->findEntity<Variable>("b");
    CHECK(b->type().get() == sym.S64());
    auto const c = mulFn->findEntity<Variable>("c");
    CHECK(c->type().get() == sym.F64());
    auto* result = mulFn->findEntity<Variable>("result");
    CHECK(result->type().get() == sym.S64());
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
    auto* tu = cast<TranslationUnit*>(ast.get());
    auto* fnDecl = tu->declaration<FunctionDefinition>(0);
    CHECK(fnDecl->returnType().get() == sym.S64());
    CHECK(fnDecl->parameter(0)->type().get() == sym.S64());
    CHECK(fnDecl->parameter(1)->type().get() == sym.S64());
    CHECK(fnDecl->parameter(2)->type().get() == sym.F64());
    auto* fn = tu->declaration<FunctionDefinition>(0);
    CHECK(fn->returnType().get() == sym.S64());
    CHECK(fn->parameter(0)->type().get() == sym.S64());
    CHECK(fn->parameter(1)->type().get() == sym.S64());
    CHECK(fn->parameter(2)->type().get() == sym.F64());
    CHECK(fn->parameter(3)->type().get() == sym.Byte());
    auto* varDecl = fn->body()->statement<VariableDeclaration>(0);
    CHECK(varDecl->type().get() == sym.S64());
    CHECK(varDecl->initExpr()->type().get() == sym.S64());
    auto* nestedScope = fn->body()->statement<CompoundStatement>(1);
    auto* nestedVarDecl = nestedScope->statement<VariableDeclaration>(0);
    auto* nestedvarDeclInit = cast<Literal*>(nestedVarDecl->initExpr());
    CHECK(nestedvarDeclInit->type() == QualType::Const(sym.Str()));
    CHECK(nestedvarDeclInit->valueCategory() == LValue);
    auto* xDecl = fn->body()->statement<VariableDeclaration>(2);
    CHECK(xDecl->type().get() == sym.S64());
    auto* intLit = cast<ast::Literal*>(xDecl->initExpr());
    CHECK(intLit->value<APInt>() == 39);
    auto* zDecl = fn->body()->statement<VariableDeclaration>(3);
    CHECK(zDecl->type().get() == sym.S64());
    auto* intHexLit = cast<ast::Literal*>(zDecl->initExpr());
    CHECK(intHexLit->value<APInt>() == 0x39E);
    auto* yDecl = fn->body()->statement<VariableDeclaration>(4);
    CHECK(yDecl->type().get() == sym.F64());
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
    auto* tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu);
    auto* calleeDecl = tu->declaration<FunctionDefinition>(1);
    REQUIRE(calleeDecl);
    CHECK(calleeDecl->returnType().get() == sym.F32());
    CHECK(calleeDecl->parameter(0)->type().get() == sym.F32());
    CHECK(calleeDecl->parameter(1)->type().get() == sym.S64());
    CHECK(calleeDecl->parameter(2)->type().get() == sym.Bool());
    auto* caller = tu->declaration<FunctionDefinition>(0);
    auto* resultDecl = caller->body()->statement<VariableDeclaration>(0);
    CHECK(resultDecl->initExpr()->type().get() == sym.F32());
    auto* fnCallExpr = cast<ast::FunctionCall*>(resultDecl->initExpr());
    auto const& calleeOverloadSet = sym.lookup<OverloadSet>("callee");
    REQUIRE(calleeOverloadSet);
    auto* calleeFunction = calleeOverloadSet->front();
    CHECK(fnCallExpr->function() == calleeFunction);
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
    auto* tu = cast<TranslationUnit*>(ast.get());
    auto* xDef = tu->declaration<StructDefinition>(0);
    CHECK(xDef->name() == "X");
    auto* iDecl = xDef->body()->statement<VariableDeclaration>(0);
    CHECK(iDecl->name() == "i");
    CHECK(iDecl->type().get() == sym.F32());
    CHECK(iDecl->offset() == 0);
    CHECK(iDecl->index() == 0);
    auto* jDecl = xDef->body()->statement<VariableDeclaration>(1);
    CHECK(jDecl->name() == "j");
    CHECK(jDecl->type().get() == sym.S64());
    CHECK(jDecl->offset() == 8);
    CHECK(jDecl->index() == 1);
    auto* b2Decl = xDef->body()->statement<VariableDeclaration>(3);
    CHECK(b2Decl->name() == "b2");
    CHECK(b2Decl->type().get() == sym.Bool());
    CHECK(b2Decl->offset() == 17);
    CHECK(b2Decl->index() == 3);
    auto* fDef = xDef->body()->statement<FunctionDefinition>(4);
    CHECK(fDef->name() == "f");
    CHECK(fDef->returnType().get() == sym.Byte());
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
    auto* tu = cast<TranslationUnit*>(ast.get());
    auto* f = tu->declaration<FunctionDefinition>(0);
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
    auto* x = sym.lookup<Scope>("X");
    sym.pushScope(x);
    auto* fOS = sym.lookup<OverloadSet>("f");
    auto const* fFn = fOS->front();
    /// Finding `f` in the overload set with `X.Y` as argument shall succeed.
    CHECK(fFn->argumentType(0)->parent()->name() == "X");
    sym.popScope();
}

TEST_CASE("Conditional operator", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
fn main(i: int) -> int {
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

TEST_CASE("Universal function call syntax", "[sema]") {
    SECTION("Non-member") {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    // fn f(&this)  {}
}
fn f(x: &X) {}
fn main() {
    var x: X;
    x.f();
})");
        REQUIRE(iss.empty());
        auto* tu = cast<TranslationUnit*>(ast.get());
        auto decls = tu->declarations();
        auto* mainDecl = *ranges::find_if(decls, [](auto* decl) {
            return decl->name() == "main";
        });
        auto* main = cast<FunctionDefinition*>(mainDecl);
        auto* stmt = main->body()->statement<ExpressionStatement>(1);
        auto* call = cast<FunctionCall*>(stmt->expression());
        auto* f = call->callee()->entity();
        CHECK(f->name() == "f");
        CHECK(isa<GlobalScope>(f->parent()));
    }
    SECTION("Member") {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    fn f(&this)  {}
}
fn f(x: &X) {}
fn main() {
    var x: X;
    x.f();
})");
        REQUIRE(iss.empty());
        auto* tu = cast<TranslationUnit*>(ast.get());
        auto decls = tu->declarations();
        auto* mainDecl = *ranges::find_if(decls, [](auto* decl) {
            return decl->name() == "main";
        });
        auto* main = cast<FunctionDefinition*>(mainDecl);
        auto* stmt = main->body()->statement<ExpressionStatement>(1);
        auto* call = cast<FunctionCall*>(stmt->expression());
        auto* f = call->callee()->entity();
        CHECK(f->name() == "f");
        CHECK(f->parent()->name() == "X");
    }
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
})");
    REQUIRE(iss.empty());
    auto* x = sym.lookup<StructType>("X");
    CHECK(x->size() == 8);
    CHECK(x->align() == 8);
    auto* y = sym.lookup<StructType>("Y");
    CHECK(y->size() == 16);
    CHECK(y->align() == 8);
    auto* z = sym.lookup<StructType>("Z");
    CHECK(z->size() == 7);
    CHECK(z->align() == 1);
    auto* w = sym.lookup<StructType>("W");
    CHECK(w->size() == 16);
    CHECK(w->align() == 8);
}
