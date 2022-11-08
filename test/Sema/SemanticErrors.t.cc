#include <Catch/Catch2.hpp>

#include "Parser/SyntaxIssue.h"
#include "Sema/SemanticIssue.h"
#include "test/IssueHelper.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Use of undeclared identifier", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() -> int { return x; }
fn f(param: UnknownID) {}
fn g() { let v: UnknownType; }
fn h() { 1 + x; }
fn i() { let y: X.Z; }
struct X { struct Y {} }
)");
    CHECK(issues.findOnLine<UseOfUndeclaredIdentifier>(2));
    CHECK(issues.findOnLine<UseOfUndeclaredIdentifier>(3));
    CHECK(issues.findOnLine<UseOfUndeclaredIdentifier>(4));
    CHECK(issues.findOnLine<UseOfUndeclaredIdentifier>(5));
    CHECK(issues.findOnLine<UseOfUndeclaredIdentifier>(6));
}

TEST_CASE("Bad symbol reference", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() -> int {
	let i = int;
	let j: 0 = int;
	return int;
}
fn f() -> 0 {}
fn f(i: 0) {}
)");
    CHECK(issues.findOnLine<BadSymbolReference>(3));
    CHECK(issues.findOnLine<BadSymbolReference>(4, 9));
    CHECK(issues.findOnLine<BadSymbolReference>(4, 13));
    CHECK(issues.findOnLine<BadSymbolReference>(5));
    CHECK(issues.findOnLine<BadSymbolReference>(7));
    CHECK(issues.findOnLine<BadSymbolReference>(8));
}

TEST_CASE("Invalid redefinition of builtin types", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X {
	fn int() {}
	struct float {}
})");
    CHECK(issues.findOnLine<InvalidDeclaration>(3).has_value());
    CHECK(issues.findOnLine<InvalidDeclaration>(4).has_value());
}

TEST_CASE("Bad type conversion", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { let x: float = 1; }
fn f(x: int) { let y: float = 1.; }
fn f(x: float) -> int { return "a string"; }
)");
    auto const line2  = issues.findOnLine<BadTypeConversion>(2);
    REQUIRE(line2);
    CHECK(line2->from() == issues.sym.Int());
    CHECK(line2->to() == issues.sym.Float());
    CHECK(issues.noneOnLine(3));
    auto const line4 = issues.findOnLine<BadTypeConversion>(4);
    REQUIRE(line4);
    CHECK(line4->from() == issues.sym.String());
    CHECK(line4->to() == issues.sym.Int());
}

TEST_CASE("Bad operands for expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main(i: int) -> bool {
	let a = !(i == 1.0);
	let b = !(i + 1.0);
	let c = !i;
	let d = ~i;
})");
    auto const line3  = issues.findOnLine<BadOperandsForBinaryExpression>(3);
    REQUIRE(line3);
    CHECK(line3->lhs() == issues.sym.Int());
    CHECK(line3->rhs() == issues.sym.Float());
    auto const line4 = issues.findOnLine<BadOperandsForBinaryExpression>(4);
    REQUIRE(line4);
    CHECK(line4->lhs() == issues.sym.Int());
    CHECK(line4->rhs() == issues.sym.Float());
    auto const line5 = issues.findOnLine<BadOperandForUnaryExpression>(5);
    REQUIRE(line5);
    CHECK(line5->operand() == issues.sym.Int());
    CHECK(issues.noneOnLine(6));
}

TEST_CASE("Bad function call expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { X.callee(); }
fn g() { X.callee(0); }
struct X {
	fn callee(a: string) {}
}
)");
    auto const line2  = issues.findOnLine<BadFunctionCall>(2);
    REQUIRE(line2);
    CHECK(line2->reason() == BadFunctionCall::Reason::NoMatchingFunction);
    auto const line3 = issues.findOnLine<BadFunctionCall>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == BadFunctionCall::Reason::NoMatchingFunction);
}

TEST_CASE("Bad member access expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() {
//  These aren't actually semantic issues but parsing issues.
//  Although we might consider allowing X.<0, 1...> in the future.
//  X.0;
//  X."0";
//  X.0.0;
	X.data;
}
struct X{ let data: float; }
)");
    //  CHECK(issues.findOnLine<BadMemberAccess>(3));
    //  CHECK(issues.findOnLine<BadMemberAccess>(4));
    //  CHECK(issues.findOnLine<BadMemberAccess>(5));
    CHECK(issues.noneOnLine(6));
}

TEST_CASE("Invalid function redefinition", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {}
fn f() -> int {}
fn g() {}
fn g() {}
)");
    auto const line3  = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::CantOverloadOnReturnType);
    CHECK(line3->symbolCategory() == SymbolCategory::Function);
    auto const line5 = issues.findOnLine<InvalidDeclaration>(5);
    REQUIRE(line5);
    CHECK(line5->reason() == InvalidDeclaration::Reason::Redefinition);
    CHECK(line5->symbolCategory() == SymbolCategory::Function);
}

TEST_CASE("Invalid variable redefinition", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f(x: int) {
	{ let x: float; }
	let x: float;
}
fn f(x: int, x: int) {}
)");
    CHECK(issues.noneOnLine(3));
    auto const line4 = issues.findOnLine<InvalidDeclaration>(4);
    REQUIRE(line4);
    CHECK(line4->reason() == InvalidDeclaration::Reason::Redefinition);
    CHECK(line4->symbolCategory() == SymbolCategory::Variable);
    CHECK(line4->existingSymbolCategory() == SymbolCategory::Variable);
    auto const line6 = issues.findOnLine<InvalidDeclaration>(6);
    REQUIRE(line6);
    CHECK(line6->reason() == InvalidDeclaration::Reason::Redefinition);
    CHECK(line6->symbolCategory() == SymbolCategory::Variable);
    CHECK(line6->existingSymbolCategory() == SymbolCategory::Variable);
}

TEST_CASE("Invalid redefinition category", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct f{}
fn f(){}
fn g(){}
struct g{}
)");
    auto const line3  = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::Redefinition);
    CHECK(line3->symbolCategory() == SymbolCategory::Function);
    CHECK(line3->existingSymbolCategory() == SymbolCategory::ObjectType);
    auto const line5 = issues.findOnLine<InvalidDeclaration>(5);
    REQUIRE(line5);
    CHECK(line5->reason() == InvalidDeclaration::Reason::Redefinition);
    CHECK(line5->symbolCategory() == SymbolCategory::ObjectType);
    CHECK(line5->existingSymbolCategory() == SymbolCategory::OverloadSet);
}

TEST_CASE("Invalid variable declaration", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {
	let v;
	let x = 0;
	let y: x;
	let z = int;
}
fn g(y: Y.data) {}
struct Y { var data: int; }
)");
    // let v;
    auto const line3 = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::CantInferType);
    // let x = 0;
    CHECK(issues.noneOnLine(4));
    // let y: x;
    auto const line5 = issues.findOnLine<BadSymbolReference>(5);
    REQUIRE(line5);
    CHECK(line5->have() == ast::EntityCategory::Value);
    CHECK(line5->expected() == ast::EntityCategory::Type);
    // let z = int;
    auto const line6 = issues.findOnLine<BadSymbolReference>(6);
    REQUIRE(line6);
    CHECK(line6->have() == ast::EntityCategory::Type);
    CHECK(line6->expected() == ast::EntityCategory::Value);
    // fn g(y: Y.data) {}
    auto const line8 = issues.findOnLine<BadSymbolReference>(8);
    REQUIRE(line8);
    CHECK(line8->have() == ast::EntityCategory::Value);
    CHECK(line8->expected() == ast::EntityCategory::Type);
}

TEST_CASE("Invalid declaration", "[sema][issue]") {
    auto const issues  = test::getSemaIssues(R"(
fn f() {
	fn g() {}
	struct X {}
}
)");
    SymbolID const fID = issues.sym.lookupOverloadSet("f")->find(std::array<TypeID, 0>{})->symbolID();
    auto const line3   = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::InvalidInCurrentScope);
    CHECK(line3->currentScope().symbolID() == fID);
    auto const line4 = issues.findOnLine<InvalidDeclaration>(4);
    REQUIRE(line4);
    CHECK(line4->reason() == InvalidDeclaration::Reason::InvalidInCurrentScope);
    CHECK(line4->currentScope().symbolID() == fID);
}

TEST_CASE("Invalid statement at struct scope", "[sema][issue]") {
    auto const issues  = test::getSemaIssues(R"(
struct X {
	return 0;
	1;
	1 + 2;
	if (1 > 0) {}
	while (1 > 0) {}
	{}
	fn f() { {} }
})");
    SymbolID const xID = issues.sym.lookupObjectType("X")->symbolID();
    auto checkLine     = [&](int line) {
        auto const issue = issues.findOnLine<InvalidStatement>(line);
        REQUIRE(issue);
        CHECK(issue->reason() == InvalidStatement::Reason::InvalidScopeForStatement);
        CHECK(issue->currentScope().symbolID() == xID);
    };
    checkLine(3);                // return 0;
    checkLine(4);                // 1;
    checkLine(5);                // 1 + 2;
    checkLine(6);                // if (1 > 0) {}
    checkLine(7);                // while (1 > 0) {}
    checkLine(8);                // {}
    CHECK(issues.noneOnLine(9)); // fn f() { {} }
}

TEST_CASE("Cyclic dependency in struct definition - 1", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: Y; }
struct Y { var x: X; }
)");
    CHECK(issues.findOnLine<StrongReferenceCycle>(2));
}

TEST_CASE("Cyclic dependency in struct definition - 2", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X {
    var y: Y;
}
struct Y {
    var z: Z;
}
struct Z {
    var w: W;
}
struct W {
    var x: X;
})");
    CHECK(issues.findOnLine<StrongReferenceCycle>(2));
}
