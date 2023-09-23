#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "test/IssueHelper.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;
using namespace sema;
using enum BadExpr::Reason;

TEST_CASE("Use of undeclared identifier", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
/* 2 */ fn f() -> int { return x; }
/* 3 */ fn f(param: UnknownID) {}
/* 4 */ fn g() { let v: UnknownType; }
/* 5 */ fn h() { 1 + x; }
/* 6 */ fn i() { let y: X.Z; }
/* 7 */ struct X { struct Y {} }
/* 8 */ struct Z { var i: in; }
)");
    CHECK(issues.findOnLine<BadExpr>(2, UndeclaredID));
    CHECK(issues.findOnLine<BadExpr>(3, UndeclaredID));
    CHECK(issues.findOnLine<BadExpr>(4, UndeclaredID));
    CHECK(issues.findOnLine<BadExpr>(5, UndeclaredID));
    CHECK(issues.findOnLine<BadExpr>(6, UndeclaredID));
    CHECK(issues.noneOnLine(7));
    CHECK(issues.findOnLine<BadExpr>(8, UndeclaredID));
}

TEST_CASE("Bad symbol reference", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() -> int {
	let i = int;
	let j: 0
         = int;
	return int;
}
fn f() -> 0 {}
fn f(i: 0) {}
)");
    CHECK(issues.findOnLine<BadSymRef>(3));
    CHECK(issues.findOnLine<BadSymRef>(4));
    CHECK(issues.findOnLine<BadSymRef>(5));
    CHECK(issues.findOnLine<BadSymRef>(6));
    CHECK(issues.findOnLine<BadSymRef>(8));
    CHECK(issues.findOnLine<BadSymRef>(9));
}

TEST_CASE("Invalid redefinition of builtin types", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X {
	fn int() {}
	struct float {}
})");
    auto* line3 = issues.findOnLine<GenericBadStmt>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == GenericBadStmt::ReservedIdentifier);
    auto* line4 = issues.findOnLine<GenericBadStmt>(4);
    REQUIRE(line4);
    CHECK(line4->reason() == GenericBadStmt::ReservedIdentifier);
}

TEST_CASE("Bad type conversion", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { let x: float = 1; }
fn f(x: int) { let y: float = 1.; }
fn f(x: float) -> int { return "a string"; }
)");
    CHECK(issues.noneOnLine(3));
    auto const line4 = issues.findOnLine<BadTypeConv>(4);
    REQUIRE(line4);
    CHECK(line4->to() == issues.sym.S64());
}

TEST_CASE("Bad operands for unary expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main(i: int) -> bool {
/* 3 */	!i;
/* 4 */	~i;
/* 5 */ ++i;
/* 6 */ --0;
})");
    CHECK(issues.findOnLine<BadExpr>(3, UnaryExprBadType));
    CHECK(issues.noneOnLine(4));
    CHECK(issues.findOnLine<BadExpr>(5, UnaryExprImmutable));
    CHECK(issues.findOnLine<BadExpr>(6, UnaryExprValueCat));
}

TEST_CASE("Bad operands for binary expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main(i: int, f: double) -> bool {
/* 3 */ i == 1.0;
/* 4 */ i + '1';
/* 5 */ f ^ 1.0;
/* 6 */ i *= 2;
/* 7 */ 2 *= 2;
})");
    CHECK(issues.findOnLine<BadExpr>(3, BinaryExprNoCommonType));
    CHECK(issues.findOnLine<BadExpr>(4, BinaryExprNoCommonType));
    CHECK(issues.findOnLine<BadExpr>(5, BinaryExprBadType));
    CHECK(issues.findOnLine<BadExpr>(6, BinaryExprImmutableLHS));
    CHECK(issues.findOnLine<BadExpr>(7, BinaryExprValueCatLHS));
}

TEST_CASE("Bad function call expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() { X.callee(); }
fn g() { X.callee(0); }
struct X {
	fn callee(a: string) {}
})");
    auto const line2 = issues.findOnLine<ORError>(2, ORError::NoMatch);
    CHECK(line2);
    auto const line3 = issues.findOnLine<ORError>(3, ORError::NoMatch);
    CHECK(line3);
}

TEST_CASE("Bad member access expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() {
/* 3 */ X.data;
/* 4 */
/* 5 */
/* 6 */
/* 7 */
}
struct X { let data: float; }
)");
    CHECK(issues.findOnLine<BadExpr>(3, MemAccNonStaticThroughType));
}

TEST_CASE("Invalid function redefinition", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {}
fn f() -> int {}
fn g() {}
fn g() {}
)");
    auto* line3 = issues.findOnLine<Redefinition>(3);
    REQUIRE(line3);
    CHECK(isa<Function>(line3->existing()));
    auto* line5 = issues.findOnLine<Redefinition>(5);
    REQUIRE(line5);
    CHECK(isa<Function>(line5->existing()));
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
    auto* line4 = issues.findOnLine<Redefinition>(4);
    CHECK(line4);
    auto* line6 = issues.findOnLine<Redefinition>(6);
    CHECK(line6);
}

TEST_CASE("Invalid redefinition category", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct f{}
fn f(){}
fn g(){}
struct g{}
)");
    auto const line3 = issues.findOnLine<Redefinition>(3);
    REQUIRE(line3);
    CHECK(isa<StructType>(line3->existing()));
    auto const line5 = issues.findOnLine<Redefinition>(5);
    REQUIRE(line5);
    CHECK(isa<OverloadSet>(line5->existing()));
}

TEST_CASE("Invalid variable declaration", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
/* 2 */ fn f() {
/* 3 */    let v;
/* 4 */    let x = 0;
/* 5 */    let y: x;
/* 6 */    let z = int;
/* 7 */ }
)");
    // let v;
    CHECK(issues.findOnLine<BadVarDecl>(3, BadVarDecl::CantInferType));
    CHECK(issues.noneOnLine(4));
    auto const line5 = issues.findOnLine<BadSymRef>(5);
    REQUIRE(line5);
    CHECK(line5->have() == EntityCategory::Value);
    CHECK(line5->expected() == EntityCategory::Type);
    auto const line6 = issues.findOnLine<BadSymRef>(6);
    REQUIRE(line6);
    CHECK(line6->have() == EntityCategory::Type);
    CHECK(line6->expected() == EntityCategory::Value);
}

TEST_CASE("Invalid declaration", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() {
	fn g() {}
	struct X {}
})");
    Function const* f = issues.sym.lookup<OverloadSet>("f")->front();
    auto const line3 = issues.findOnLine<GenericBadStmt>(3);
    REQUIRE(line3);
    CHECK(line3->scope() == f);
    CHECK(line3->reason() == GenericBadStmt::InvalidScope);
    auto const line4 = issues.findOnLine<GenericBadStmt>(4);
    REQUIRE(line4);
    CHECK(line4->scope() == f);
    CHECK(line4->reason() == GenericBadStmt::InvalidScope);
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
        auto const issue = issues.findOnLine<GenericBadStmt>(line);
        REQUIRE(issue);
        CHECK(issue->reason() == GenericBadStmt::InvalidScope);
        CHECK(issue->scope() == x);
    };
    checkLine(3);                // return 0;
    checkLine(4);                // 1;
    checkLine(5);                // 1 + 2;
    checkLine(6);                // if (1 > 0) {}
    checkLine(7);                // while (1 > 0) {}
    checkLine(8);                // {}
    CHECK(issues.noneOnLine(9)); // fn f() { {} }
}

TEST_CASE("Cyclic dependency in struct definition", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: Y; }
struct Y { var x: X; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("No cyclic dependency issues with pointers", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: *Y; }
struct Y { var x: *X; }
)");
    CHECK(issues.empty());
}

TEST_CASE("Cyclic dependency in struct definition (larger cycle)",
          "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: Y; }
struct Y { var z: Z; }
struct Z { var w: W; }
struct W { var x: X; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("Cyclic dependency in struct definition with arrays",
          "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
struct X { var y: [Y, 2]; }
struct Y { var x: [X, 1]; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("Non void function must return a value", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() -> int { return; }
)");
    auto issue = issues.findOnLine<BadReturnStmt>(2);
    REQUIRE(issue);
    CHECK(issue->reason() == BadReturnStmt::NonVoidMustReturnValue);
}

TEST_CASE("Void function must not return a value", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f() -> void { return 0; }
)");
    auto issue = issues.findOnLine<BadReturnStmt>(2);
    REQUIRE(issue);
    CHECK(issue->reason() == BadReturnStmt::VoidMustNotReturnValue);
}

TEST_CASE("Expect reference initializer", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() { var r: &mut int = 1; }
)");
    auto issue = issues.findOnLine<BadValueCatConv>(2);
    CHECK(issue);
}

TEST_CASE("Invalid lists", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main() {
/* 3 */ let a = [u32(1), 0.0];
/* 4 */ let b = [u32(1), int];
/* 5 */ let c = [];
/* 6 */ let d: [int, 1, int];
})");

    CHECK(issues.findOnLine<BadExpr>(3, ListExprNoCommonType));
    auto badSymRef = issues.findOnLine<BadSymRef>(4);
    REQUIRE(badSymRef);
    CHECK(badSymRef->have() == EntityCategory::Type);
    CHECK(badSymRef->expected() == EntityCategory::Value);
    CHECK(issues.findOnLine<BadExpr>(5, GenericBadExpr));
    CHECK(issues.findOnLine<BadExpr>(6, ListExprTypeExcessElements));
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
    CHECK(issues.findOnLine<GenericBadStmt>(3, GenericBadStmt::InvalidScope));
    CHECK(issues.findOnLine<GenericBadStmt>(5, GenericBadStmt::InvalidScope));
    CHECK(issues.noneOnLine(8));
    CHECK(issues.noneOnLine(9));
    CHECK(issues.noneOnLine(13));
}

TEST_CASE("Invalid this parameter", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn f(this) {}
fn f(n: int, this) {}
struct X {
    fn f(n: int, this) {}
}
)");
    CHECK(issues.findOnLine<BadVarDecl>(2, BadVarDecl::ThisInFreeFunction));
    CHECK(issues.findOnLine<BadVarDecl>(3, BadVarDecl::ThisInFreeFunction));
    CHECK(issues.findOnLine<BadVarDecl>(5, BadVarDecl::ThisPosition));
}

TEST_CASE("Invalid special member functions", "[sema][issue]") {
    return;
    auto const issues = test::getSemaIssues(R"(
/*  2 */  fn new() {}
/*  3 */  struct X {
/*  4 */      fn new() {}
/*  5 */      fn new(&this) {}
/*  6 */      fn new(self: &mut X) {}
/*  7 */      fn new(lhs: &mut X, rhs: &X) {}
/*  8 */      fn move(lhs: &mut X) {}
/*  9 */      fn move(lhs: &mut X, rhs: &mut X) {}
/* 10 */      fn delete(&mut this, n: int) {}
/* 11 */      fn delete(&mut this) {}
/* 12 */      fn new(&mut this) -> int {}
/*    */  }
)");
    CHECK(issues.findOnLine<BadSMF>(2, BadSMF::NotInStruct));
    CHECK(issues.findOnLine<BadSMF>(4, BadSMF::NoParams));
    CHECK(issues.findOnLine<BadSMF>(5, BadSMF::BadFirstParam));
    CHECK(issues.noneOnLine(6));
    CHECK(issues.noneOnLine(7));
    CHECK(issues.findOnLine<BadSMF>(8, BadSMF::MoveSignature));
    CHECK(issues.noneOnLine(9));
    CHECK(issues.findOnLine<BadSMF>(10, BadSMF::DeleteSignature));
    CHECK(issues.noneOnLine(11));
    CHECK(issues.findOnLine<BadSMF>(12, BadSMF::HasReturnType));
}

TEST_CASE("Explicit calls to SMFs", "[sema][issue]") {
    return;
    auto const issues = test::getSemaIssues(R"(
fn main() {
/*  3 */ var x = X();
/*  4 */ x.new();
/*  5 */ var y = x;
/*  6 */ x.new(y);
}
struct X {
    fn new(&mut this) {}
    fn new(&mut this, rhs: &X) {}
    fn delete(&mut this) {}
})");
    CHECK(issues.findOnLine<BadExpr>(4, BadExpr::ExplicitSMFCall));
    CHECK(issues.findOnLine<BadExpr>(6, BadExpr::ExplicitSMFCall));
}

TEST_CASE("Illegal value passing", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
/*  2 */ fn foo(n: void) {}
/*  3 */ fn bar(n: [int]) { bar(); }
/*  4 */ fn baz() -> [int] {}
/*  5 */ fn quux() {
/*  6 */     let data = [1, 2, 3];
/*  7 */     let p: *[int] = &data;
/*  8 */     return *p;
/*  9 */ }
/* 10 */ fn quuz() { return; }
/* 11 */ fn frob() -> void {}
)");
    CHECK(issues.findOnLine<BadPassedType>(2, BadPassedType::Argument));
    CHECK(issues.findOnLine<BadPassedType>(3, BadPassedType::Argument));
    CHECK(issues.findOnLine<BadPassedType>(4, BadPassedType::Return));
    CHECK(issues.findOnLine<BadPassedType>(8, BadPassedType::ReturnDeduced));
    CHECK(issues.noneOnLine(10));
    CHECK(issues.noneOnLine(11));
}
