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
        mul->find(std::array{ sym.qualInt(), sym.qualInt(), sym.qualFloat() });
    REQUIRE(mulFn);
    auto const& fnType = mulFn->signature();
    CHECK(fnType.returnType()->base() == sym.Int());
    REQUIRE(fnType.argumentCount() == 3);
    CHECK(fnType.argumentType(0)->base() == sym.Int());
    CHECK(fnType.argumentType(1)->base() == sym.Int());
    CHECK(fnType.argumentType(2)->base() == sym.Float());
    auto* a = mulFn->findEntity<Variable>("a");
    CHECK(a->type()->base() == sym.Int());
    auto* b = mulFn->findEntity<Variable>("b");
    CHECK(b->type()->base() == sym.Int());
    auto const c = mulFn->findEntity<Variable>("c");
    CHECK(c->type()->base() == sym.Float());
    auto* result = mulFn->findEntity<Variable>("result");
    CHECK(result->type()->base() == sym.Int());
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
    auto* fnDecl = cast<FunctionDefinition*>(tu->declarations[0].get());
    CHECK(fnDecl->returnType()->base() == sym.Int());
    CHECK(fnDecl->parameters[0]->type()->base() == sym.Int());
    CHECK(fnDecl->parameters[1]->type()->base() == sym.Int());
    CHECK(fnDecl->parameters[2]->type()->base() == sym.Float());
    CHECK(fnDecl->parameters[3]->type()->base() == sym.String());
    auto* fn = cast<FunctionDefinition*>(tu->declarations[0].get());
    CHECK(fn->returnType()->base() == sym.Int());
    CHECK(fn->parameters[0]->type()->base() == sym.Int());
    CHECK(fn->parameters[1]->type()->base() == sym.Int());
    CHECK(fn->parameters[2]->type()->base() == sym.Float());
    CHECK(fn->parameters[3]->type()->base() == sym.String());
    auto* varDecl = cast<VariableDeclaration*>(fn->body->statements[0].get());
    CHECK(varDecl->type()->base() == sym.Int());
    auto* varDeclInit = cast<Identifier*>(varDecl->initExpression.get());
    CHECK(varDeclInit->type()->base() == sym.Int());
    CHECK(varDeclInit->valueCategory() == ValueCategory::LValue);
    auto* nestedScope = cast<CompoundStatement*>(fn->body->statements[1].get());
    auto* nestedVarDecl =
        cast<VariableDeclaration*>(nestedScope->statements[0].get());
    CHECK(nestedVarDecl->type()->base() == sym.String());
    auto* nestedvarDeclInit =
        cast<Literal*>(nestedVarDecl->initExpression.get());
    CHECK(nestedvarDeclInit->type()->base() == sym.String());
    CHECK(nestedvarDeclInit->valueCategory() == ValueCategory::RValue);
    auto* xDecl = cast<VariableDeclaration*>(fn->body->statements[2].get());
    CHECK(xDecl->type()->base() == sym.Int());
    auto* intLit = cast<Literal*>(xDecl->initExpression.get());
    CHECK(intLit->value<LiteralKind::Integer>() == 39);
    CHECK(intLit->valueCategory() == ValueCategory::RValue);
    auto* zDecl = cast<VariableDeclaration*>(fn->body->statements[3].get());
    CHECK(zDecl->type()->base() == sym.Int());
    auto* intHexLit = cast<Literal*>(zDecl->initExpression.get());
    CHECK(intHexLit->value<LiteralKind::Integer>() == 0x39E);
    auto* yDecl = cast<VariableDeclaration*>(fn->body->statements[4].get());
    CHECK(yDecl->type()->base() == sym.Float());
    auto* floatLit = cast<Literal*>(yDecl->initExpression.get());
    CHECK(floatLit->value<LiteralKind::FloatingPoint>().to<f64>() == 1.2);
    auto* ret           = cast<ReturnStatement*>(fn->body->statements[5].get());
    auto* retIdentifier = cast<Identifier*>(ret->expression.get());
    CHECK(retIdentifier->type()->base() == sym.Int());
    CHECK(retIdentifier->valueCategory() == ValueCategory::LValue);
}

TEST_CASE("Decoration of the AST with function call expression", "[sema]") {
    auto const text      = R"(
fn caller() -> float {
	let result = callee("Hello world", 0, true);
	return result;
}
fn callee(a: string, b: int, c: bool) -> float { return 0.0; }
)";
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(text);
    REQUIRE(iss.empty());
    auto* tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu);
    auto* calleeDecl = cast<FunctionDefinition*>(tu->declarations[1].get());
    REQUIRE(calleeDecl);
    CHECK(calleeDecl->returnType()->base() == sym.Float());
    CHECK(calleeDecl->parameters[0]->type()->base() == sym.String());
    CHECK(calleeDecl->parameters[1]->type()->base() == sym.Int());
    CHECK(calleeDecl->parameters[2]->type()->base() == sym.Bool());
    auto* caller = cast<FunctionDefinition*>(tu->declarations[0].get());
    auto* resultDecl =
        cast<VariableDeclaration*>(caller->body->statements[0].get());
    CHECK(resultDecl->initExpression->type()->base() == sym.Float());
    auto* fnCallExpr = cast<FunctionCall*>(resultDecl->initExpression.get());
    auto const& calleeOverloadSet = sym.lookup<OverloadSet>("callee");
    REQUIRE(calleeOverloadSet != nullptr);
    auto* calleeFunction = calleeOverloadSet->find(
        std::array{ sym.qualString(), sym.qualInt(), sym.qualBool() });
    REQUIRE(calleeFunction != nullptr);
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
    auto* xDef = cast<StructDefinition*>(tu->declarations[0].get());
    CHECK(xDef->name() == "X");
    auto* iDecl = cast<VariableDeclaration*>(xDef->body->statements[0].get());
    CHECK(iDecl->name() == "i");
    CHECK(iDecl->type()->base() == sym.Float());
    CHECK(iDecl->offset() == 0);
    CHECK(iDecl->index() == 0);
    auto* jDecl = cast<VariableDeclaration*>(xDef->body->statements[1].get());
    CHECK(jDecl->name() == "j");
    CHECK(jDecl->type()->base() == sym.Int());
    CHECK(jDecl->offset() == 8);
    CHECK(jDecl->index() == 1);
    auto* b2Decl = cast<VariableDeclaration*>(xDef->body->statements[3].get());
    CHECK(b2Decl->name() == "b2");
    CHECK(b2Decl->type()->base() == sym.Bool());
    CHECK(b2Decl->offset() == 17);
    CHECK(b2Decl->index() == 3);
    auto* fDef = cast<FunctionDefinition*>(xDef->body->statements[4].get());
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
    auto const* tu = cast<TranslationUnit*>(ast.get());
    auto const* f  = cast<FunctionDefinition*>(tu->declarations[0].get());
    auto const* y  = cast<VariableDeclaration*>(f->body->statements[0].get());
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
    auto const* x_y_type = sym.qualify(sym.lookup<Type>("Y"));
    auto const* fFn      = fOS->find(std::array{ x_y_type });
    /// Finding `f` in the overload set with `X.Y` as argument shall succeed.
    CHECK(fFn != nullptr);
    sym.popScope();
    auto const* y_type        = sym.qualify(sym.lookup<Type>("Y"));
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
        auto const* tu = cast<TranslationUnit*>(ast.get());
        auto* mainDecl = ranges::find_if(tu->declarations, [](auto& decl) {
                             return decl->name() == "main";
                         })->get();
        auto* main     = cast<FunctionDefinition*>(mainDecl);
        auto* stmt =
            cast<ExpressionStatement*>(main->body->statements[1].get());
        auto* call = cast<FunctionCall*>(stmt->expression.get());
        auto* f    = call->object->entity();
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
        auto const* tu = cast<TranslationUnit*>(ast.get());
        auto* mainDecl = ranges::find_if(tu->declarations, [](auto& decl) {
                             return decl->name() == "main";
                         })->get();
        auto* main     = cast<FunctionDefinition*>(mainDecl);
        auto* stmt =
            cast<ExpressionStatement*>(main->body->statements[1].get());
        auto* call = cast<FunctionCall*>(stmt->expression.get());
        auto* f    = call->object->entity();
        CHECK(f->name() == "f");
        CHECK(f->parent()->name() == "X");
    }
}
