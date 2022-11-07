#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "Parser/SyntaxIssue.h"
#include "test/Parser/SimpleParser.h"
#include "test/IssueHelper.h"

using namespace scatha;
using namespace ast;

TEST_CASE("Parse simple function", "[parse]") {
    std::string const text = R"(
fn mul(a: int, b: X.Y.Z) -> int {
	var result = a;
	return result;
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = downCast<TranslationUnit>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = downCast<FunctionDefinition>(tu->declarations[0].get());
    CHECK(function->name() == "mul");
    REQUIRE(function->parameters.size() == 2);
    CHECK(function->parameters[0]->name() == "a");
    auto const* aTypeExpr = downCast<Identifier>(function->parameters[0]->typeExpr.get());
    CHECK(aTypeExpr->token().id == "int");
    CHECK(function->parameters[1]->name() == "b");
    auto const* bTypeExpr = downCast<MemberAccess>(function->parameters[1]->typeExpr.get());
    CHECK(bTypeExpr->object->nodeType() == NodeType::MemberAccess);
    auto const* bTypeExprLhs = downCast<MemberAccess>(bTypeExpr->object.get());
    CHECK(bTypeExprLhs->object->nodeType() == NodeType::Identifier);
    CHECK(bTypeExprLhs->object->token().id == "X");
    CHECK(bTypeExprLhs->member->nodeType() == NodeType::Identifier);
    CHECK(bTypeExprLhs->member->token().id == "Y");
    CHECK(bTypeExpr->member->nodeType() == NodeType::Identifier);
    CHECK(bTypeExpr->member->token().id == "Z");
    auto const* returnTypeExpr = downCast<Identifier>(function->returnTypeExpr.get());
    CHECK(returnTypeExpr->token().id == "int");
    CompoundStatement* const body = function->body.get();
    REQUIRE(body->statements.size() == 2);
    auto* const resultDecl = downCast<VariableDeclaration>(body->statements[0].get());
    CHECK(resultDecl->name() == "result");
    CHECK(!resultDecl->typeExpr);
    CHECK(!resultDecl->isConstant);
    CHECK(resultDecl->initExpression->nodeType() == scatha::ast::NodeType::Identifier);
    auto* const returnStatement = downCast<ReturnStatement>(body->statements[1].get());
    CHECK(returnStatement->expression->nodeType() == scatha::ast::NodeType::Identifier);
}

TEST_CASE("Parse literals", "[parse]") {
    std::string const text = R"(
fn main() -> void {
	let a: int = 39;
	let b = 1.2;
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu         = downCast<TranslationUnit>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = downCast<FunctionDefinition>(tu->declarations[0].get());
    CHECK(function->name() == "main");
    auto* const aDecl  = downCast<VariableDeclaration>(function->body->statements[0].get());
    auto* const intLit = downCast<IntegerLiteral>(aDecl->initExpression.get());
    CHECK(intLit->token().id == "39");
    CHECK(intLit->value == 39);
    auto* const bDecl    = downCast<VariableDeclaration>(function->body->statements[1].get());
    auto* const floatLit = downCast<FloatingPointLiteral>(bDecl->initExpression.get());
    CHECK(floatLit->token().id == "1.2");
    CHECK(floatLit->value == 1.2);
}

TEST_CASE("Parse last statement ending with '}'", "[parse]") {
    std::string const text = R"(
fn main() {
    {}
})";
    auto const [ast, iss] = test::parse(text);
    CHECK(iss.empty());
}

TEST_CASE("Parse conditional", "[parse]") {
    auto const [ast, iss] = test::parse("fn main() { true ? 1 : 4; }");
    CHECK(iss.empty());
}

// Not a parsing but a semantic issue!
//TEST_CASE("Parse invalid variable decl", "[parse][issue]") {
//    std::string const text = R"(
//fn main() {
//    var result: int * float = a;
//})";
//    CHECK_THROWS_AS(makeAST(text), SyntaxIssue);
//};

//TEST_CASE("Parse invalid member access", "[parse][issue]") {
//    std::string const text = R"(
//fn main() {
//    j.;
//})";
//    auto issues = test::getSemaIssues(text);
//    CHECK(issues.findOnLine<>(<#size_t line#>)
//
//    CHECK_THROWS_AS(makeAST(text), SyntaxIssue);
//};
//
