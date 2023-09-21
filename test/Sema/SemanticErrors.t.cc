#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
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
    CHECK(issues.findOnLine<InvalidDeclaration>(3));
    CHECK(issues.findOnLine<InvalidDeclaration>(4));
}

TEST_CASE("Bad type conversion", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { let x: float = 1; }
fn f(x: int) { let y: float = 1.; }
fn f(x: float) -> int { return "a string"; }
)");
    CHECK(issues.noneOnLine(3));
    auto const line4 = issues.findOnLine<BadTypeConversion>(4);
    REQUIRE(line4);
    CHECK(line4->to().get() == issues.sym.S64());
}

TEST_CASE("Bad operands for expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main(i: int) -> bool {
	let a = !(i == 1.0);
	let b = !(i + 1.0);
	let c = !i;
	let d = ~i;
})");
    auto const line3 = issues.findOnLine<BadOperandsForBinaryExpression>(3);
    REQUIRE(line3);
    CHECK(line3->lhs().get() == issues.sym.S64());
    CHECK(line3->rhs().get() == issues.sym.F64());
    auto const line4 = issues.findOnLine<BadOperandsForBinaryExpression>(4);
    REQUIRE(line4);
    CHECK(line4->lhs().get() == issues.sym.S64());
    CHECK(line4->rhs().get() == issues.sym.F64());
    auto const line5 = issues.findOnLine<BadOperandForUnaryExpression>(5);
    REQUIRE(line5);
    CHECK(line5->operandType().get() == issues.sym.S64());
    CHECK(issues.noneOnLine(6));
}

TEST_CASE("Bad function call expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { X.callee(); }
fn g() { X.callee(0); }
struct X {
	fn callee(a: string) {}
})");
    auto const line2 = issues.findOnLine<NoMatchingFunction>(2);
    CHECK(line2);
    auto const line3 = issues.findOnLine<NoMatchingFunction>(3);
    CHECK(line3);
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
struct X { let data: float; }
)");
    // CHECK(issues.findOnLine<BadMemberAccess>(3));
    // CHECK(issues.findOnLine<BadMemberAccess>(4));
    // CHECK(issues.findOnLine<BadMemberAccess>(5));
    CHECK(issues.noneOnLine(6));
}

TEST_CASE("Invalid function redefinition", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {}
fn f() -> int {}
fn g() {}
fn g() {}
)");
    auto const line3 = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::Redefinition);
    auto const line5 = issues.findOnLine<InvalidDeclaration>(5);
    REQUIRE(line5);
    CHECK(line5->reason() == InvalidDeclaration::Reason::Redefinition);
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
    auto const line6 = issues.findOnLine<InvalidDeclaration>(6);
    REQUIRE(line6);
    CHECK(line6->reason() == InvalidDeclaration::Reason::Redefinition);
}

TEST_CASE("Invalid redefinition category", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct f{}
fn f(){}
fn g(){}
struct g{}
)");
    auto const line3 = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::Redefinition);
    // CHECK(line3->symbolCategory() == SymbolCategory::Function);
    // CHECK(line3->existingSymbolCategory() == SymbolCategory::Type);
    auto const line5 = issues.findOnLine<InvalidDeclaration>(5);
    REQUIRE(line5);
    CHECK(line5->reason() == InvalidDeclaration::Reason::Redefinition);
    // CHECK(line5->symbolCategory() == SymbolCategory::Type);
    // CHECK(line5->existingSymbolCategory() == SymbolCategory::OverloadSet);
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
    CHECK(line5->have() == EntityCategory::Value);
    CHECK(line5->expected() == EntityCategory::Type);
    // let z = int;
    auto const line6 = issues.findOnLine<BadSymbolReference>(6);
    REQUIRE(line6);
    CHECK(line6->have() == EntityCategory::Type);
    CHECK(line6->expected() == EntityCategory::Value);
    // fn g(y: Y.data) {}
    auto const line8 = issues.findOnLine<BadSymbolReference>(8);
    REQUIRE(line8);
    CHECK(line8->have() == EntityCategory::Value);
    CHECK(line8->expected() == EntityCategory::Type);
}

TEST_CASE("Invalid declaration", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {
	fn g() {}
	struct X {}
})");
    Function const* f = issues.sym.lookup<OverloadSet>("f")->front();
    auto const line3 = issues.findOnLine<InvalidDeclaration>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == InvalidDeclaration::Reason::InvalidInCurrentScope);
    CHECK(line3->currentScope() == f);
    auto const line4 = issues.findOnLine<InvalidDeclaration>(4);
    REQUIRE(line4);
    CHECK(line4->reason() == InvalidDeclaration::Reason::InvalidInCurrentScope);
    CHECK(line4->currentScope() == f);
}

TEST_CASE("Invalid statement at struct scope", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X {
	return 0;
	1;
	1 + 2;
	if (1 > 0) {}
	while (1 > 0) {}
	{}
	fn f() { {} }
})");
    auto const* x = issues.sym.lookup<ObjectType>("X");
    auto checkLine = [&](int line) {
        auto const issue = issues.findOnLine<InvalidStatement>(line);
        REQUIRE(issue);
        CHECK(issue->reason() ==
              InvalidStatement::Reason::InvalidScopeForStatement);
        CHECK(issue->currentScope() == x);
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

TEST_CASE("No cyclic dependency issues with pointers", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: *Y; }
struct Y { var x: *X; }
)");
    CHECK(issues.empty());
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

TEST_CASE("Non void function must return a value", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() -> int { return; }
)");
    auto issue = issues.findOnLine<InvalidStatement>(2);
    REQUIRE(issue);
    CHECK(issue->reason() ==
          InvalidStatement::Reason::NonVoidFunctionMustReturnAValue);
}

TEST_CASE("Void function must not return a value", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() -> void { return 0; }
)");
    auto issue = issues.findOnLine<InvalidStatement>(2);
    REQUIRE(issue);
    CHECK(issue->reason() ==
          InvalidStatement::Reason::VoidFunctionMustNotReturnAValue);
}

TEST_CASE("Expect reference initializer", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() { var r: &mut int = 1; }
)");
    auto issue = issues.findOnLine<BadTypeConversion>(2);
    CHECK(issue);
}

TEST_CASE("Invalid lists", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() {
    let a = [u32(1), 0.0];
    let b = [u32(1), int];
    let c = [];
    let d: [int, 1, int];
})");

    auto cmnType = issues.findOnLine<InvalidListExpr>(3);
    REQUIRE(cmnType);
    CHECK(cmnType->reason() == InvalidListExpr::NoCommonType);

    auto badSymRef = issues.findOnLine<BadSymbolReference>(4);
    REQUIRE(badSymRef);
    CHECK(badSymRef->have() == EntityCategory::Type);
    CHECK(badSymRef->expected() == EntityCategory::Value);

    auto invalidDecl = issues.findOnLine<InvalidDeclaration>(5);
    REQUIRE(invalidDecl);
    CHECK(invalidDecl->reason() == InvalidDeclaration::Reason::CantInferType);

    auto invCount = issues.findOnLine<InvalidListExpr>(6);
    REQUIRE(invCount);
    CHECK(invCount->reason() == InvalidListExpr::InvalidElemCountForArrayType);
}

TEST_CASE("Invalid jump", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() {
    break;
    if 1 == 0 {
        continue;
    }
    for i = 0; i < 10; ++i {
        break;
        continue;
    }
    while true {
        if 1 == 2 {
            break;
        }
    }
})");
    CHECK(issues.findOnLine<InvalidStatement>(3));
    CHECK(issues.findOnLine<InvalidStatement>(5));
    CHECK(issues.noneOnLine(8));
    CHECK(issues.noneOnLine(9));
    CHECK(issues.noneOnLine(13));
}
