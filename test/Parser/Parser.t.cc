#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "test/IssueHelper.h"
#include "test/Parser/SimpleParser.h"

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
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations().size() == 1);
    auto* const function = tu->declaration<FunctionDefinition>(0);
    CHECK(function->name() == "mul");
    REQUIRE(function->parameters().size() == 2);
    CHECK(function->parameters()[0]->name() == "a");
    auto const* aTypeExpr =
        cast<Identifier*>(function->parameters()[0]->typeExpr());
    CHECK(aTypeExpr->value() == "int");
    CHECK(function->parameters()[1]->name() == "b");
    auto const* bTypeExpr =
        cast<MemberAccess*>(function->parameters()[1]->typeExpr());
    auto const* bTypeExprLhs =
        dyncast<MemberAccess const*>(bTypeExpr->object());
    REQUIRE(bTypeExprLhs);
    CHECK(cast<Identifier const*>(bTypeExprLhs->object())->value() == "X");
    CHECK(cast<Identifier const*>(bTypeExprLhs->member())->value() == "Y");
    CHECK(cast<Identifier const*>(bTypeExpr->member())->value() == "Z");
    auto const* returnTypeExpr = cast<Identifier*>(function->returnTypeExpr());
    CHECK(returnTypeExpr->value() == "int");
    CompoundStatement* const body = function->body();
    REQUIRE(body->statements().size() == 2);
    auto* const resultDecl = cast<VariableDeclaration*>(body->statements()[0]);
    CHECK(resultDecl->name() == "result");
    CHECK(!resultDecl->typeExpr());
    // CHECK(!resultDecl->isConstant);
    CHECK(isa<Identifier>(resultDecl->initExpression()));
    auto* const returnStatement =
        cast<ReturnStatement const*>(body->statements()[1]);
    CHECK(isa<Identifier>(returnStatement->expression()));
}

TEST_CASE("Parse literals", "[parse]") {
    std::string const text = R"(
fn main() -> void {
	let a: int = 39;
	let b = 1.2;
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations().size() == 1);
    auto* const function = tu->declaration<FunctionDefinition>(0);
    CHECK(function->name() == "main");
    auto* const aDecl =
        cast<VariableDeclaration*>(function->body()->statements()[0]);
    auto* const intLit = cast<Literal const*>(aDecl->initExpression());
    CHECK(intLit->value<APInt>() == 39);
    auto* const bDecl =
        cast<VariableDeclaration const*>(function->body()->statements()[1]);
    auto* const floatLit = cast<Literal const*>(bDecl->initExpression());
    CHECK(floatLit->value<APFloat>().to<f64>() == 1.2);
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

TEST_CASE("Parse while statement", "[parse]") {
    std::string const text = R"(
fn test() {
    while x < 0 {
        x += 1;
    }
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations().size() == 1);
    auto* const function = tu->declaration<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const whileStatement =
        cast<LoopStatement const*>(body->statements()[0]);
    REQUIRE(whileStatement);
    auto* const condition =
        cast<BinaryExpression const*>(whileStatement->condition());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement = cast<ExpressionStatement const*>(
        whileStatement->block()->statements()[0]);
    REQUIRE(exprStatement);
    auto* const expr =
        cast<BinaryExpression const*>(exprStatement->expression());
    REQUIRE(expr);
    CHECK(expr->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier const*>(expr->lhs());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<Literal const*>(expr->rhs());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value<APInt>() == 1);
}

TEST_CASE("Parse do-while statement", "[parse]") {
    std::string const text = R"(
fn test() {
    do {
        x += 1;
    } while x < 0;
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations().size() == 1);
    auto* const function = tu->declaration<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const doWhileStatement =
        cast<LoopStatement const*>(body->statements()[0]);
    REQUIRE(doWhileStatement);
    auto* const condition =
        cast<BinaryExpression const*>(doWhileStatement->condition());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement = cast<ExpressionStatement const*>(
        doWhileStatement->block()->statements()[0]);
    REQUIRE(exprStatement);
    auto* const expr =
        cast<BinaryExpression const*>(exprStatement->expression());
    REQUIRE(expr);
    CHECK(expr->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier const*>(expr->lhs());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<Literal const*>(expr->rhs());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value<APInt>() == 1);
}

TEST_CASE("Parse for statement", "[parse]") {
    std::string const text = R"(
fn test() {
    for x = 0; x < 10; x += 1 {
        print(x);
    }
})";
    auto const [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations().size() == 1);
    auto* const function = tu->declaration<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const forStatement =
        cast<LoopStatement const*>(body->statements()[0]);
    REQUIRE(forStatement);
    auto* const varDecl =
        cast<VariableDeclaration const*>(forStatement->varDecl());
    REQUIRE(varDecl);
    CHECK(varDecl->name() == "x");
    CHECK(varDecl->typeExpr() == nullptr);
    auto* const varInitExpr = cast<Literal const*>(varDecl->initExpression());
    REQUIRE(varInitExpr);
    CHECK(varInitExpr->value<APInt>() == 0);
    auto* const condition =
        cast<BinaryExpression const*>(forStatement->condition());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const increment =
        cast<BinaryExpression const*>(forStatement->increment());
    REQUIRE(increment);
    CHECK(increment->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier const*>(increment->lhs());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<Literal const*>(increment->rhs());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value<APInt>() == 1);
    auto* const loopStatement = cast<ExpressionStatement const*>(
        forStatement->block()->statements()[0]);
    REQUIRE(loopStatement);
    auto* const functionCall =
        cast<FunctionCall const*>(loopStatement->expression());
    REQUIRE(functionCall);
    CHECK(cast<Identifier const*>(functionCall->object())->value() == "print");
}
