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

TEST_CASE("Registration in SymbolTable", "[sema]") {
    auto const text      = R"(
fn mul(a: int, b: int, c: float) -> int {
	let result = a;
	return result;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* mul = sym.lookup<OverloadSet>("mul");
    REQUIRE(mul);
    auto const* mulFn =
        mul->find(std::array{ sym.qS64(), sym.qS64(), sym.qFloat() });
    REQUIRE(mulFn);
    auto const& fnType = mulFn->signature();
    CHECK(fnType.returnType()->base() == sym.S64());
    REQUIRE(fnType.argumentCount() == 3);
    CHECK(fnType.argumentType(0)->base() == sym.S64());
    CHECK(fnType.argumentType(1)->base() == sym.S64());
    CHECK(fnType.argumentType(2)->base() == sym.Float());
    auto* a = mulFn->findEntity<Variable>("a");
    CHECK(a->type()->base() == sym.S64());
    auto* b = mulFn->findEntity<Variable>("b");
    CHECK(b->type()->base() == sym.S64());
    auto const c = mulFn->findEntity<Variable>("c");
    CHECK(c->type()->base() == sym.Float());
    auto* result = mulFn->findEntity<Variable>("result");
    CHECK(result->type()->base() == sym.S64());
}

TEST_CASE("Decoration of the AST") {
    auto const text      = R"(
fn mul(a: int, b: int, c: float, d: string) -> int {
	let result = a;
	{ // declaration of variable of the same name in a nested scope
		var result = "some string";
	}
	// declaration of float variable
	let y = 39;
	let z = 0x39E;
	let x = 1.2;
	return result;
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* tu     = cast<TranslationUnit*>(ast.get());
    auto* fnDecl = tu->declaration<FunctionDefinition>(0);
    CHECK(fnDecl->returnType()->base() == sym.S64());
    CHECK(fnDecl->parameter(0)->type()->base() == sym.S64());
    CHECK(fnDecl->parameter(1)->type()->base() == sym.S64());
    CHECK(fnDecl->parameter(2)->type()->base() == sym.Float());
    // CHECK(fnDecl->parameters()[3]->type()->base() == sym.String());
    auto* fn = tu->declaration<FunctionDefinition>(0);
    CHECK(fn->returnType()->base() == sym.S64());
    CHECK(fn->parameter(0)->type()->base() == sym.S64());
    CHECK(fn->parameter(1)->type()->base() == sym.S64());
    CHECK(fn->parameter(2)->type()->base() == sym.Float());
    CHECK(fn->parameter(3)->type()->base() == sym.String());
    auto* varDecl = fn->body()->statement<VariableDeclaration>(0);
    CHECK(varDecl->type()->base() == sym.S64());
    auto* varDeclInit = cast<Identifier*>(varDecl->initExpression());
    CHECK(varDeclInit->type()->base() == sym.S64());
    CHECK(varDeclInit->valueCategory() == ValueCategory::LValue);
    auto* nestedScope   = fn->body()->statement<CompoundStatement>(1);
    auto* nestedVarDecl = nestedScope->statement<VariableDeclaration>(0);
    // CHECK(nestedVarDecl->type()->base() == sym.String());
    auto* nestedvarDeclInit = cast<Literal*>(nestedVarDecl->initExpression());
    // CHECK(nestedvarDeclInit->type()->base() == sym.String());
    CHECK(nestedvarDeclInit->valueCategory() == ValueCategory::RValue);
    auto* xDecl = fn->body()->statement<VariableDeclaration>(2);
    CHECK(xDecl->type()->base() == sym.S64());
    auto* intLit = cast<Literal*>(xDecl->initExpression());
    CHECK(intLit->value<LiteralKind::Integer>() == 39);
    CHECK(intLit->valueCategory() == ValueCategory::RValue);
    auto* zDecl = fn->body()->statement<VariableDeclaration>(3);
    CHECK(zDecl->type()->base() == sym.S64());
    auto* intHexLit = cast<Literal*>(zDecl->initExpression());
    CHECK(intHexLit->value<LiteralKind::Integer>() == 0x39E);
    auto* yDecl = fn->body()->statement<VariableDeclaration>(4);
    CHECK(yDecl->type()->base() == sym.Float());
    auto* floatLit = cast<Literal*>(yDecl->initExpression());
    CHECK(floatLit->value<LiteralKind::FloatingPoint>().to<f64>() == 1.2);
    auto* ret           = fn->body()->statement<ReturnStatement>(5);
    auto* retIdentifier = cast<Identifier*>(ret->expression());
    CHECK(retIdentifier->type()->base() == sym.S64());
    CHECK(retIdentifier->valueCategory() == ValueCategory::LValue);
}

TEST_CASE("Decoration of the AST with function call expression", "[sema]") {
    auto const text      = R"(
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
    CHECK(calleeDecl->returnType()->base() == sym.Float());
    CHECK(calleeDecl->parameter(0)->type()->base() == sym.Float());
    CHECK(calleeDecl->parameter(1)->type()->base() == sym.S64());
    CHECK(calleeDecl->parameter(2)->type()->base() == sym.Bool());
    auto* caller     = tu->declaration<FunctionDefinition>(0);
    auto* resultDecl = caller->body()->statement<VariableDeclaration>(0);
    CHECK(resultDecl->initExpression()->type()->base() == sym.Float());
    auto* fnCallExpr = cast<FunctionCall*>(resultDecl->initExpression());
    auto const& calleeOverloadSet = sym.lookup<OverloadSet>("callee");
    REQUIRE(calleeOverloadSet);
    auto* calleeFunction = calleeOverloadSet->find(
        std::array{ sym.qFloat(), sym.qS64(), sym.qBool() });
    REQUIRE(calleeFunction);
    CHECK(fnCallExpr->function() == calleeFunction);
    CHECK(fnCallExpr->valueCategory() == ValueCategory::RValue);
}

TEST_CASE("Decoration of the AST with struct definition", "[sema]") {
    auto const text      = R"(
struct X {
	var i: float;
	var j: int = 0;
	var b1: bool = true;
	var b2: bool = true;
	fn f(x: int, y: int) -> string {}
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* tu   = cast<TranslationUnit*>(ast.get());
    auto* xDef = tu->declaration<StructDefinition>(0);
    CHECK(xDef->name() == "X");
    auto* iDecl = xDef->body()->statement<VariableDeclaration>(0);
    CHECK(iDecl->name() == "i");
    CHECK(iDecl->type()->base() == sym.Float());
    CHECK(iDecl->offset() == 0);
    CHECK(iDecl->index() == 0);
    auto* jDecl = xDef->body()->statement<VariableDeclaration>(1);
    CHECK(jDecl->name() == "j");
    CHECK(jDecl->type()->base() == sym.S64());
    CHECK(jDecl->offset() == 8);
    CHECK(jDecl->index() == 1);
    auto* b2Decl = xDef->body()->statement<VariableDeclaration>(3);
    CHECK(b2Decl->name() == "b2");
    CHECK(b2Decl->type()->base() == sym.Bool());
    CHECK(b2Decl->offset() == 17);
    CHECK(b2Decl->index() == 3);
    auto* fDef = xDef->body()->statement<FunctionDefinition>(4);
    CHECK(fDef->name() == "f");
    /// TODO: Test argument types when we properly recognize member functions as
    /// having an implicit 'this' argument
    CHECK(fDef->returnType()->base() == sym.String());
}

TEST_CASE("Member access into undeclared struct", "[sema]") {
    auto const text      = R"(
fn f(x: X) -> int { return x.data; }
struct X { var data: int; }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
}

TEST_CASE("Type reference access into undeclared struct", "[sema]") {
    auto const text            = R"(
fn f() {
	let y: X.Y;
}
struct X { struct Y {} }
)";
    auto const [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto const* tu    = cast<TranslationUnit*>(ast.get());
    auto const* f     = tu->declaration<FunctionDefinition>(0);
    auto const* y     = f->body()->statement<VariableDeclaration>(0);
    auto const* YType = y->type();
    CHECK(YType->base()->name() == "Y");
    CHECK(YType->base()->parent()->name() == "X");
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
    auto const text      = R"(
struct Y {}
struct X {
	fn f(y: Y) {}
	struct Y{}
})";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* x = sym.lookup<Scope>("X");
    sym.pushScope(x);
    auto* fOS            = sym.lookup<OverloadSet>("f");
    auto const* x_y_type = sym.qualify(sym.lookup<StructureType>("Y"));
    auto const* fFn      = fOS->find(std::array{ x_y_type });
    /// Finding `f` in the overload set with `X.Y` as argument shall succeed.
    CHECK(fFn != nullptr);
    sym.popScope();
    auto const* y_type        = sym.qualify(sym.lookup<StructureType>("Y"));
    auto const* undeclaredfFn = fOS->find({ { y_type } });
    /// Finding `f` with `Y` (global) as argument shall fail.
    CHECK(undeclaredfFn == nullptr);
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
public fn main() {
    var x: X;
    x.f();
})");
        REQUIRE(iss.empty());
        auto* tu       = cast<TranslationUnit*>(ast.get());
        auto decls     = tu->declarations();
        auto* mainDecl = *ranges::find_if(decls, [](auto* decl) {
            return decl->name() == "main";
        });
        auto* main     = cast<FunctionDefinition*>(mainDecl);
        auto* stmt     = main->body()->statement<ExpressionStatement>(1);
        auto* call     = cast<FunctionCall*>(stmt->expression());
        auto* f        = call->object()->entity();
        CHECK(f->name() == "f");
        CHECK(isa<GlobalScope>(f->parent()));
    }
    SECTION("Member") {
        auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    fn f(&this)  {}
}
fn f(x: &X) {}
public fn main() {
    var x: X;
    x.f();
})");
        REQUIRE(iss.empty());
        auto* tu       = cast<TranslationUnit*>(ast.get());
        auto decls     = tu->declarations();
        auto* mainDecl = *ranges::find_if(decls, [](auto* decl) {
            return decl->name() == "main";
        });
        auto* main     = cast<FunctionDefinition*>(mainDecl);
        auto* stmt     = main->body()->statement<ExpressionStatement>(1);
        auto* call     = cast<FunctionCall*>(stmt->expression());
        auto* f        = call->object()->entity();
        CHECK(f->name() == "f");
        CHECK(f->parent()->name() == "X");
    }
}

TEST_CASE("Sizeof structs with reference and array members", "[sema]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct X {
    var r: &s8;
}
struct Y {
    var r: &[s8];
}
struct Z {
    var r: [s8, 7];
})");
    REQUIRE(iss.empty());
    auto* x = sym.lookup<StructureType>("X");
    CHECK(x->size() == 8);
    CHECK(x->align() == 8);
    auto* y = sym.lookup<StructureType>("Y");
    CHECK(y->size() == 16);
    CHECK(y->align() == 8);
    auto* z = sym.lookup<StructureType>("Z");
    CHECK(z->size() == 7);
    CHECK(z->align() == 1);
}
