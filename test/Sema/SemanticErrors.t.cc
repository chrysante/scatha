#include <Catch/Catch2.hpp>

#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SemanticIssuesNEW.h"
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
    CHECK(issues.findOnLine<BadIdentifier>(2));
    CHECK(issues.findOnLine<BadIdentifier>(3));
    CHECK(issues.findOnLine<BadIdentifier>(4));
    CHECK(issues.findOnLine<BadIdentifier>(5));
    CHECK(issues.findOnLine<BadIdentifier>(6));
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
    auto const line4 = issues.findOnLine<BadTypeConversion>(4);
    REQUIRE(line4);
    CHECK(line4->to().get() == issues.sym.S64());
}

TEST_CASE("Bad operands for unary expression", "[sema][issue]") {
    auto const issues = test::getSemaIssues(R"(
fn main(i: int) -> bool {
/* 3 */	!i;
/* 4 */	~i;
/* 5 */ ++i;
/* 6 */ --0;
})");
    {
        auto const issue = issues.findOnLine<BadUnaryExpr>(3);
        REQUIRE(issue);
        CHECK(issue->reason() == BadUnaryExpr::Type);
    }
    CHECK(issues.noneOnLine(4));
    {
        auto const issue = issues.findOnLine<BadUnaryExpr>(5);
        REQUIRE(issue);
        CHECK(issue->reason() == BadUnaryExpr::Immutable);
    }
    {
        auto const issue = issues.findOnLine<BadUnaryExpr>(6);
        REQUIRE(issue);
        CHECK(issue->reason() == BadUnaryExpr::ValueCat);
    }
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
    {
        auto const issue = issues.findOnLine<BadBinaryExpr>(3);
        REQUIRE(issue);
        CHECK(issue->reason() == BadBinaryExpr::NoCommonType);
    }
    {
        auto const issue = issues.findOnLine<BadBinaryExpr>(4);
        REQUIRE(issue);
        CHECK(issue->reason() == BadBinaryExpr::NoCommonType);
    }
    {
        auto const issue = issues.findOnLine<BadBinaryExpr>(5);
        REQUIRE(issue);
        CHECK(issue->reason() == BadBinaryExpr::BadType);
    }
    {
        auto const issue = issues.findOnLine<BadBinaryExpr>(6);
        REQUIRE(issue);
        CHECK(issue->reason() == BadBinaryExpr::ImmutableLHS);
    }
    {
        auto const issue = issues.findOnLine<BadBinaryExpr>(7);
        REQUIRE(issue);
        CHECK(issue->reason() == BadBinaryExpr::ValueCatLHS);
    }
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
    auto const line3 = issues.findOnLine<BadVarDecl>(3);
    REQUIRE(line3);
    CHECK(line3->reason() == BadVarDecl::CantInferType);
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

TEST_CASE("Cyclic dependency in struct definition - 1", "[sema][issue]") {
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

    auto invalidDecl = issues.findOnLine<BadVarDecl>(5);
    REQUIRE(invalidDecl);
    CHECK(invalidDecl->reason() == BadVarDecl::CantInferType);

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
    CHECK(issues.findOnLine<GenericBadStmt>(3));
    CHECK(issues.findOnLine<GenericBadStmt>(5));
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
    {
        auto* issue = issues.findOnLine<BadVarDecl>(2);
        REQUIRE(issue);
        CHECK(issue->reason() == BadVarDecl::ThisInFreeFunction);
    }
    {
        auto* issue = issues.findOnLine<BadVarDecl>(3);
        REQUIRE(issue);
        CHECK(issue->reason() == BadVarDecl::ThisInFreeFunction);
    }
    {
        auto* issue = issues.findOnLine<BadVarDecl>(5);
        REQUIRE(issue);
        CHECK(issue->reason() == BadVarDecl::ThisPosition);
    }
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
    {
        auto* issue = issues.findOnLine<BadSMF>(2);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::NotInStruct);
    }
    {
        auto* issue = issues.findOnLine<BadSMF>(4);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::NoParams);
    }
    {
        auto* issue = issues.findOnLine<BadSMF>(5);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::BadFirstParam);
    }
    CHECK(issues.noneOnLine(6));
    CHECK(issues.noneOnLine(7));
    {
        auto* issue = issues.findOnLine<BadSMF>(8);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::MoveSignature);
    }
    CHECK(issues.noneOnLine(9));
    {
        auto* issue = issues.findOnLine<BadSMF>(10);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::DeleteSignature);
    }
    CHECK(issues.noneOnLine(11));
    {
        auto* issue = issues.findOnLine<BadSMF>(12);
        REQUIRE(issue);
        CHECK(issue->reason() == BadSMF::HasReturnType);
    }
}
