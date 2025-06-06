#include <catch2/catch_test_macros.hpp>

#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SimpleAnalzyer.h"
#include "Util/IssueHelper.h"
#include "Util/LibUtil.h"

using namespace scatha;
using namespace sema;
using enum BadExpr::Reason;

TEST_CASE("Use of undeclared identifier", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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

TEST_CASE("Undeclared ID in FFI", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
extern "C" fn f(x: X) -> void;
extern "C" fn g() -> X;
)");
    CHECK(issues.findOnLine<BadExpr>(2, UndeclaredID));
    CHECK(issues.findOnLine<BadExpr>(3, UndeclaredID));
}

TEST_CASE("FFI redefinition", "[sema][issues]") {
#if 0 // TODO: Implement this behaviour
    auto issues = test::getSemaIssues(R"(
extern "C" fn f() -> void;
extern "C" fn f(x: int) -> void;
)");
    CHECK(issues.findOnLine<Redefinition>(3));
#endif
}

TEST_CASE("Bad symbol reference", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
/* 2 */ fn f() { let x: float = 1; }
/* 3 */ fn f(x: int) { let y: float = 1.; }
/* 4 */ fn f(x: float) -> int { return "a string"; }
)");
    CHECK(issues.noneOnLine(2));
    CHECK(issues.noneOnLine(3));
    auto const line4 = issues.findOnLine<BadTypeConv>(4);
    REQUIRE(line4);
    CHECK(line4->to() == issues.sym.S64());
}

TEST_CASE("Bad reinterpreting conversions", "[sema][issue]") {
    auto const iss = test::getSemaIssues(R"(
/* 2 */ fn f(n: int) { reinterpret<Nontriv>(n); }
/* 3 */ fn g(n: int) { reinterpret<Triv>(n); }
/* 4 */ fn h(n: int) { reinterpret<TrivSmall>(n); }
public struct Nontriv {
    fn delete(&mut this) {}
    var n: int;
}
public struct Triv {
    var n: int;
}
public struct TrivSmall {
    var n: u8;
}
)");
    CHECK(iss.findOnLine<BadExpr>(2));
    CHECK(iss.noneOnLine(3));
    CHECK(iss.findOnLine<BadExpr>(4));
}

TEST_CASE("Bad operands for unary expression", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    CHECK(issues.findOnLine<BadExpr>(6, AssignExprImmutableLHS));
    CHECK(issues.findOnLine<BadExpr>(7, AssignExprValueCatLHS));
}

TEST_CASE("Bad function call expression", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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
    CHECK(isa<Function>(line5->existing()));
}

TEST_CASE("Invalid variable declaration", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/*  2 */ protocol P;
/*  3 */ fn f() {
/*  4 */     let v;
/*  5 */     let x = 0;
/*  6 */     let y: x;
/*  7 */     let z = int;
/*  8 */     let p: P;
/*  9 */ }
/* 10 */ struct S {
/* 11 */     var v: void;
/* 12 */     var p: P;
/* 13 */     var r: &int;
/* 14 */ }
)");
    CHECK(issues.findOnLine<BadVarDecl>(4, BadVarDecl::CantInferType));
    CHECK(issues.noneOnLine(5));
    auto* line6 = issues.findOnLine<BadSymRef>(6);
    REQUIRE(line6);
    CHECK(line6->have() == EntityCategory::Value);
    CHECK(line6->expected() == EntityCategory::Type);
    auto* line7 = issues.findOnLine<BadSymRef>(7);
    REQUIRE(line7);
    CHECK(line7->have() == EntityCategory::Type);
    CHECK(line7->expected() == EntityCategory::Value);
    CHECK(issues.findOnLine<BadVarDecl>(8, BadVarDecl::ProtocolType));
    CHECK(issues.findOnLine<BadVarDecl>(11, BadVarDecl::IncompleteType));
    CHECK(issues.findOnLine<BadVarDecl>(12, BadVarDecl::ProtocolType));
    CHECK(issues.findOnLine<BadVarDecl>(13, BadVarDecl::RefInStruct));
}

TEST_CASE("Invalid declaration", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/* 2 */ fn f() {
/* 3 */ 	fn g() {}
/* 4 */ 	struct X {}
/* 5 */ })");
    auto const* f = stripAlias(issues.sym.unqualifiedLookup("f").front());
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
    auto issues = test::getSemaIssues(R"(
/*  2 */ struct X {
/*  3 */     return 0;
/*  4 */     1;
/*  5 */     1 + 2;
/*  6 */     if (1 > 0) {}
/*  7 */     while (1 > 0) {}
/*  8 */     {}
/*  9 */     fn f() { {} }
/* 10 */ })");
    auto const* x = stripAlias(issues.sym.unqualifiedLookup("X").front());
    auto checkLine = [&](int line) {
        auto const issue = issues.findOnLine<GenericBadStmt>(line);
        REQUIRE(issue);
        CHECK(issue->reason() == GenericBadStmt::InvalidScope);
        CHECK(issue->scope() == x);
    };
    checkLine(3);
    checkLine(4);
    checkLine(5);
    checkLine(6);
    checkLine(7);
    checkLine(8);
    CHECK(issues.noneOnLine(9));
}

TEST_CASE("Cyclic dependency in struct definition", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X { var y: Y; }
struct Y { var x: X; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("No cyclic dependency issues with pointers", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X { var y: *Y; }
struct Y { var x: *X; }
)");
    CHECK(issues.empty());
}

TEST_CASE("Cyclic dependency in struct definition (larger cycle)",
          "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X { var y: Y; }
struct Y { var z: Z; }
struct Z { var w: W; }
struct W { var x: X; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("Cyclic dependency in struct definition with arrays",
          "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X { var y: [Y, 2]; }
struct Y { var x: [X, 1]; }
)");
    CHECK(issues.findOnLine<StructDefCycle>(0));
}

TEST_CASE("Non void function must return a value", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
fn f() -> int { return; }
)");
    auto issue = issues.findOnLine<BadReturnStmt>(2);
    REQUIRE(issue);
    CHECK(issue->reason() == BadReturnStmt::NonVoidMustReturnValue);
}

TEST_CASE("Void function must not return a value", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
fn f() -> void { return 0; }
)");
    auto issue = issues.findOnLine<BadReturnStmt>(2);
    REQUIRE(issue);
    CHECK(issue->reason() == BadReturnStmt::VoidMustNotReturnValue);
}

TEST_CASE("Expect reference initializer", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
fn main() { var r: &mut int = 1; }
)");
    auto issue = issues.findOnLine<BadValueCatConv>(2);
    CHECK(issue);
}

TEST_CASE("Invalid lists", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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

TEST_CASE("Invalid use of dynamic array", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/*  2 */ fn main() {
/*  3 */     var arr1: *unique mut [int] = unique [1, 2, 3];
/*  4 */     var arr2: *unique mut [int] = unique [1, 2, 3];
/*  5 */     move *arr1;
/*  6 */     *arr2 = *arr1;
/*  7 */     var value = *arr1;
/*  8 */
/*  9 */     let p = unique [int](1, 2);
/* 10 */     let q = unique [int](true);
/* 11 */
/* 12 */     var n = 1;
/* 13 */     [int](n);
/* 14 */ })");
    CHECK(issues.findOnLine<BadExpr>(5, MoveExprIncompleteType));
    CHECK(issues.findOnLine<BadExpr>(6, AssignExprIncompleteLHS));
    CHECK(issues.findOnLine<BadExpr>(6, AssignExprIncompleteRHS));
    CHECK(issues.findOnLine<BadVarDecl>(7, BadVarDecl::IncompleteType));
    CHECK(issues.findOnLine<BadExpr>(9, BadExpr::DynArrayConstrBadArgs));
    CHECK(issues.findOnLine<BadExpr>(10, BadExpr::DynArrayConstrBadArgs));
    CHECK(issues.findOnLine<BadExpr>(13, BadExpr::DynArrayConstrAutoStorage));
}

TEST_CASE("Invalid jump", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/*  2 */ fn main() {
/*  3 */     break;
/*  4 */     if 1 == 0 {
/*  5 */         continue;
/*  6 */     }
/*  7 */     for i = 0; i < 10; ++i {
/*  8 */         break;
/*  9 */     }
/* 10 */     for i = 0; i < 10; ++i {
/* 11 */         continue;
/* 12 */     }
/* 13 */     while true {
/* 14 */         if 1 != 2 {
/* 15 */             break;
/* 16 */         }
/* 17 */     }
/* 18 */ }
)");
    CHECK(issues.findOnLine<GenericBadStmt>(3, GenericBadStmt::InvalidScope));
    CHECK(issues.findOnLine<GenericBadStmt>(5, GenericBadStmt::InvalidScope));
    CHECK(issues.noneOnLine(8));
    CHECK(issues.noneOnLine(11));
    CHECK(issues.noneOnLine(15));
}

TEST_CASE("Invalid this parameter", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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

TEST_CASE("Bad literals", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/*  2 */ fn f() { this; }
/*  3 */ struct X { fn f() { this; } }
)");
    CHECK(issues.findOnLine<BadExpr>(2, InvalidUseOfThis));
    CHECK(issues.findOnLine<BadExpr>(3, InvalidUseOfThis));
}

TEST_CASE("Explicit calls to SMFs", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
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
    auto issues = test::getSemaIssues(R"(
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

TEST_CASE("OR Error", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X {
    fn new(&mut this, n: int) {}
}
fn main() {
/* 6 */ let x = X();
})");
    CHECK(issues.findOnLine<BadExpr>(6));
}

TEST_CASE("Compare pointers of different types", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
fn main() {
    var a = 0;
    var b = 0.0;
    &a == &b;
})");
    CHECK(issues.findOnLine<BadExpr>(5, BinaryExprNoCommonType));
}

TEST_CASE("Main must return trivial", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
struct X {
    fn new(&mut this) {}
    fn new(&mut this, rhs: &X) {}
    fn delete(&mut this) {}
}
fn main() {
    return X();
})");
    using enum BadFuncDef::Reason;
    CHECK(issues.findOnLine<BadFuncDef>(7, MainMustReturnTrivial));
}

TEST_CASE("Access data member without object", "[sema][issue]") {
    auto issues = test::getSemaIssues(R"(
/*  2 */ struct S {
/*  3 */     fn f() {
/*  4 */         i;
/*  5 */         // This case used to fly under the radar because we only tested
/*  6 */         // if the parent expression was a member access but not if the
/*  7 */         // ID was the member expression
/*  8 */         t.j;
/*  9 */     }
/* 10 */     var i: int;
/* 11 */     var t: T;
/* 12 */ }
/* 13 */ struct T { var j: int; }
)");
    CHECK(issues.findOnLine<BadExpr>(4, AccessedMemberWithoutObject));
    CHECK(issues.findOnLine<BadExpr>(8, AccessedMemberWithoutObject));
}

TEST_CASE("Redefine entity in different module", "[sema]") {
    using namespace std::string_literals;
    auto iss = test::getSemaIssues({ R"(
fn f() {}
fn g() {}
)"s,
                                     R"(
struct f {}
private struct g {} // Private declaration in a different file is not a
                    // redefinition
)"s });
    CHECK(iss.findOnLine<Redefinition>(2));
    CHECK(iss.noneOnLine(3));
}

TEST_CASE("Redefine function in different module", "[sema]") {
    using namespace std::string_literals;
    auto iss = test::getSemaIssues({ R"(
fn f(n: int) {}
fn g(n: int) {}
)"s,
                                     R"(
fn f(m: int) {}
private fn g(m: int) {}
)"s });
    CHECK(iss.findOnLine<Redefinition>(2));
    CHECK(iss.noneOnLine(3));
}

TEST_CASE("Main parameter validation", "[sema]") {
    using enum BadFuncDef::Reason;
    CHECK(test::getSemaIssues("fn main() {}").empty());
    CHECK(test::getSemaIssues("fn main(args: &[*str]) {}").empty());
    CHECK(test::getSemaIssues("fn main(n: int) {}")
              .findOnLine<BadFuncDef>(1, MainInvalidArguments));
    CHECK(test::getSemaIssues("fn main(f: float) {}")
              .findOnLine<BadFuncDef>(1, MainInvalidArguments));
}

TEST_CASE("Main access control", "[sema]") {
    CHECK(test::getSemaIssues("private fn main() {}")
              .findOnLine<BadFuncDef>(1, BadFuncDef::MainNotPublic));
    CHECK(test::getSemaIssues("public fn main() {}").empty());
}

TEST_CASE("FFI validation", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ extern "B" fn f() -> void;
/*  3 */ extern "C" fn g();
/*  4 */ extern "C" fn h(x: X) -> void;
/*  5 */ extern "C" fn h() -> X;
/*  6 */ extern "C" fn h(f: float) -> int;
/*  7 */ extern "C" fn i(f: *float) -> int;
/*  8 */ extern "C" fn i(f: *[float]) -> int;
/*  9 */ extern "C" fn i(f: int) -> *float;
struct X {}
)");
    CHECK(iss.findOnLine<BadFuncDef>(2, BadFuncDef::UnknownLinkage));
    CHECK(iss.findOnLine<BadFuncDef>(3, BadFuncDef::NoReturnType));
    CHECK(iss.noneOnLine(4));
    CHECK(iss.noneOnLine(5));
    CHECK(iss.noneOnLine(6));
    CHECK(iss.noneOnLine(7));
    CHECK(iss.noneOnLine(8));
    CHECK(iss.findOnLine<BadFuncDef>(9, BadFuncDef::InvalidReturnTypeForFFI));
}

TEST_CASE("Invalid import statements", "[sema][lib]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ import F();
/*  3 */ import A.B;
/*  4 */ use "foo";
/*  5 */ use F().A;
/*  6 */ fn foo() { import "foo"; }
/*  7 */ use DoesNotExist;
)");
    CHECK(iss.findOnLine<BadImport>(2, BadImport::InvalidExpression));
    CHECK(iss.findOnLine<BadImport>(3, BadImport::InvalidExpression));
    CHECK(iss.findOnLine<BadImport>(4, BadImport::UnscopedForeignLibImport));
    CHECK(iss.findOnLine<BadImport>(5, BadImport::InvalidExpression));
    CHECK(iss.findOnLine<GenericBadStmt>(6, GenericBadStmt::InvalidScope));
    CHECK(iss.findOnLine<BadImport>(7, BadImport::LibraryNotFound));
}

TEST_CASE("Use symbol of library imported in nested scope",
          "[lib][nativelib]") {
    test::compileLibrary("libs/testlib", "libs",
                         R"(
public fn foo() { return 42; }
)");
    auto iss = test::getSemaIssues(R"(
/*  2 */ fn test2() {
/*  3 */     { use testlib.foo; foo(); }
/*  4 */     foo();
/*  5 */     import testlib;
/*  6 */     let arr = [testlib];
/*  7 */     &testlib;
/*  8 */     *testlib;
         })",
                                   { .librarySearchPaths = { "libs" } });
    CHECK(iss.noneOnLine(3));
    CHECK(iss.findOnLine<BadExpr>(4, UndeclaredID));
    CHECK(iss.findOnLine<BadSymRef>(6));
    CHECK(iss.findOnLine<BadSymRef>(7));
    CHECK(iss.findOnLine<BadSymRef>(8));
}

TEST_CASE("Missing special member functions", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/* 2 */ public struct X { fn new(&mut this, rhs: &X) {} }
/* 3 */ public fn foo() { var x: X; }
/* 4 */ public fn foo(x: X) {}
)");
    CHECK(iss.findOnLine<BadExpr>(3, CannotConstructType));
    CHECK(iss.findOnLine<BadCleanup>(4));
}

TEST_CASE("Other object construction errors", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/* 2 */ fn foo() { return int(1, 2, 3); }
/* 3 */ fn bar() { return Inconstructible(1, 2, 3); }
/* 4 */
struct Inconstructible { fn delete(&mut this) {} }
)");
    CHECK(iss.findOnLine<BadExpr>(2, CannotConstructType));
    CHECK(iss.findOnLine<BadExpr>(3, CannotConstructType));
}

TEST_CASE("Pointer and reference to non-object type", "[sema]") {
    auto iss = test::getSemaIssues(R"(
fn foo() -> *&int {}
fn bar() -> & &int {}
)");
    CHECK(iss.findOnLine<BadExpr>(2, PointerNoObjType));
    CHECK(iss.findOnLine<BadExpr>(3, ReferenceNoObjType));
}

TEST_CASE("Type deduction mutability errors", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/*  2 */ fn a(i: int) {
/*  3 */     let m: &mut = i; // Cannot bind mutable reference to const object
/*  4 */ }
/*  5 */ fn b(i: *int) {
/*  6 */     let m: *mut = i; // Cannot convert const pointer to mutable
/*  7 */ }
/*  8 */ fn c() {
/*  9 */     let m: *unique mut = make_unique(); // Cannot convert const pointer to mutable
/* 10 */ }
/* 11 */ fn make_unique() -> *unique int { return unique int(); }
)");
    CHECK(iss.findOnLine<BadTypeDeduction>(3, BadTypeDeduction::Mutability));
    CHECK(iss.findOnLine<BadTypeDeduction>(6, BadTypeDeduction::Mutability));
    CHECK(iss.findOnLine<BadTypeDeduction>(9, BadTypeDeduction::Mutability));
}

TEST_CASE("Type deduction missing initializer", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/* 2 */ fn a() {
/* 3 */     let i: &;
/* 4 */     let j: *;
/* 5 */     let k: *mut;
/* 6 */     let l: *unique;
/* 7 */ }
)");
    using enum BadTypeDeduction::Reason;
    CHECK(iss.findOnLine<BadTypeDeduction>(3, MissingInitializer));
    CHECK(iss.findOnLine<BadTypeDeduction>(4, MissingInitializer));
    CHECK(iss.findOnLine<BadTypeDeduction>(5, MissingInitializer));
    CHECK(iss.findOnLine<BadTypeDeduction>(6, MissingInitializer));
}

TEST_CASE("Type deduction need pointer initializer", "[sema]") {
    auto iss = test::getSemaIssues(R"(
/* 2 */ fn b(i: int) {
/* 3 */     let p: * = i;
/* 4 */     let q: * = make_unique();
/* 5 */ }
/* 6 */ fn c(p: *int) {
/* 7 */     let q: *unique = p;
/* 8 */ }
/* 9 */ fn make_unique() -> *unique int { return unique int(); }
)");
    using enum BadTypeDeduction::Reason;
    CHECK(iss.findOnLine<BadTypeDeduction>(3, NoPointer));
    CHECK(iss.findOnLine<BadTypeConv>(4));
    CHECK(iss.findOnLine<BadTypeConv>(7));
}

TEST_CASE("Type deduction invalid context", "[sema]") {
    auto iss = test::getSemaIssues(R"(
struct X { var p: *; }
fn test(p: *) {}
fn test() -> * {}
)");
    using enum BadTypeDeduction::Reason;
    CHECK(iss.findOnLine<BadTypeDeduction>(2, InvalidContext));
    CHECK(iss.findOnLine<BadTypeDeduction>(3, InvalidContext));
    CHECK(iss.findOnLine<BadTypeDeduction>(4, InvalidContext));
}

TEST_CASE("Cast expr bad need pointer", "[sema]") {
    auto iss = test::getSemaIssues(R"(
fn test() { 0 as *; }
)");
    using enum BadTypeDeduction::Reason;
    CHECK(iss.findOnLine<BadTypeDeduction>(2, NoPointer));
}

TEST_CASE("Global variable without explicit type specifier", "[sema]") {
    auto iss = test::getSemaIssues(R"(
let i = 0;
)");
    CHECK(iss.findOnLine<BadVarDecl>(2, BadVarDecl::GlobalNeedsTypeSpecifier));
}

TEST_CASE("Bad fstrings", "[sema]") {
    auto iss = test::getSemaIssues(R"TEXT(
struct X {}
fn retVoid() {}
fn test() {
    "\(X())";
    "\(retVoid())";
})TEXT");
    CHECK(iss.findOnLine<BadExpr>(5));
    CHECK(iss.findOnLine<BadExpr>(6, NotFormattable));
}

TEST_CASE("Variable in protocol", "[sema]") {
    auto iss = test::getSemaIssues(R"(
protocol P {
    var i: int;
})");
    CHECK(iss.findOnLine<BadVarDecl>(3, BadVarDecl::InProtocol));
}

TEST_CASE("Invalid base classes", "[sema]") {
    auto iss = test::getSemaIssues(R"(
struct S {}
protocol P: S {}
struct T: int {}
)");
    CHECK(iss.findOnLine<BadBaseDecl>(3, BadBaseDecl::NotAProtocol));
    CHECK(iss.findOnLine<BadBaseDecl>(4, BadBaseDecl::InvalidType));
}

TEST_CASE("Ambiguous member access", "[sema]") {
    auto iss = test::getSemaIssues(R"(
struct Base1 { var n: int; }
struct Base2 { var n: int; }
struct Derived: Base1, Base2 {}
fn main() { Derived().n; }
)");
    CHECK(iss.findOnLine<BadExpr>(5, AmbiguousMemberAccess));
}

TEST_CASE("Protocol function no return type", "[sema]") {
    auto iss = test::getSemaIssues(R"(
protocol P { fn f(&this); }
)");
    CHECK(iss.findOnLine<BadFuncDef>(2, BadFuncDef::NoReturnType));
}

TEST_CASE("Invalid override", "[sema]") {
    auto iss = test::getSemaIssues(R"(
protocol P { fn f(&this) -> int; }
struct S: P { fn f(&dyn this) -> double {} }
)");
    CHECK(
        iss.findOnLine<BadFuncDef>(3,
                                   BadFuncDef::OverridingFunctionWrongRetType));
}

TEST_CASE("Construct protocol type", "[sema]") {
    auto iss = test::getSemaIssues(R"(
protocol P {}
fn test() { P(); }
)");
    CHECK(iss.findOnLine<BadExpr>(3, CannotConstructType));
}
