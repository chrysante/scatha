#include <Catch/Catch2.hpp>

#include "AST/AST.h"
#include "Parser/SyntaxIssue.h"
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
    auto const [ast, iss]  = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = cast<FunctionDefinition*>(tu->declarations[0].get());
    CHECK(function->name() == "mul");
    REQUIRE(function->parameters.size() == 2);
    CHECK(function->parameters[0]->name() == "a");
    auto const* aTypeExpr = cast<Identifier*>(function->parameters[0]->typeExpr.get());
    CHECK(aTypeExpr->token().id == "int");
    CHECK(function->parameters[1]->name() == "b");
    auto const* bTypeExpr = cast<MemberAccess*>(function->parameters[1]->typeExpr.get());
    CHECK(bTypeExpr->object->nodeType() == NodeType::MemberAccess);
    auto const* bTypeExprLhs = cast<MemberAccess*>(bTypeExpr->object.get());
    CHECK(bTypeExprLhs->object->nodeType() == NodeType::Identifier);
    CHECK(bTypeExprLhs->object->token().id == "X");
    CHECK(bTypeExprLhs->member->nodeType() == NodeType::Identifier);
    CHECK(bTypeExprLhs->member->token().id == "Y");
    CHECK(bTypeExpr->member->nodeType() == NodeType::Identifier);
    CHECK(bTypeExpr->member->token().id == "Z");
    auto const* returnTypeExpr = cast<Identifier*>(function->returnTypeExpr.get());
    CHECK(returnTypeExpr->token().id == "int");
    CompoundStatement* const body = function->body.get();
    REQUIRE(body->statements.size() == 2);
    auto* const resultDecl = cast<VariableDeclaration*>(body->statements[0].get());
    CHECK(resultDecl->name() == "result");
    CHECK(!resultDecl->typeExpr);
    CHECK(!resultDecl->isConstant);
    CHECK(resultDecl->initExpression->nodeType() == scatha::ast::NodeType::Identifier);
    auto* const returnStatement = cast<ReturnStatement*>(body->statements[1].get());
    CHECK(returnStatement->expression->nodeType() == scatha::ast::NodeType::Identifier);
}

TEST_CASE("Parse literals", "[parse]") {
    std::string const text = R"(
fn main() -> void {
	let a: int = 39;
	let b = 1.2;
})";
    auto const [ast, iss]  = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = cast<FunctionDefinition*>(tu->declarations[0].get());
    CHECK(function->name() == "main");
    auto* const aDecl  = cast<VariableDeclaration*>(function->body->statements[0].get());
    auto* const intLit = cast<IntegerLiteral*>(aDecl->initExpression.get());
    CHECK(intLit->token().id == "39");
    CHECK(intLit->value() == 39);
    auto* const bDecl    = cast<VariableDeclaration*>(function->body->statements[1].get());
    auto* const floatLit = cast<FloatingPointLiteral*>(bDecl->initExpression.get());
    CHECK(floatLit->token().id == "1.2");
    CHECK(floatLit->value() == 1.2);
}

TEST_CASE("Parse last statement ending with '}'", "[parse]") {
    std::string const text = R"(
fn main() {
    {}
})";
    auto const [ast, iss]  = test::parse(text);
    CHECK(iss.empty());
}

TEST_CASE("Parse conditional", "[parse]") {
    auto const [ast, iss] = test::parse("fn main() { true ? 1 : 4; }");
    CHECK(iss.empty());
}

// TEST_CASE("Parse invalid member access", "[parse][issue]") {
//     std::string const text = R"(
// fn main() {
//     j.;
// })";
//     auto issues = test::getSemaIssues(text);
//     CHECK(issues.findOnLine<>(<#size_t line#>)
//
//     CHECK_THROWS_AS(makeAST(text), SyntaxIssue);
// };
//

TEST_CASE("Parse while statement", "[parse]") {
    std::string const text = R"(
fn test() {
    while x < 0 {
        x += 1;
    }
})";
    auto const [ast, iss]  = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = cast<FunctionDefinition*>(tu->declarations[0].get());
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body.get();
    REQUIRE(body);
    REQUIRE(body->statements.size() == 1);
    auto* const whileStatement = cast<WhileStatement*>(body->statements[0].get());
    REQUIRE(whileStatement);
    auto* const condition = cast<BinaryExpression*>(whileStatement->condition.get());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement = cast<ExpressionStatement*>(whileStatement->block->statements[0].get());
    REQUIRE(exprStatement);
    auto* const expr = cast<BinaryExpression*>(exprStatement->expression.get());
    REQUIRE(expr);
    CHECK(expr->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier*>(expr->lhs.get());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<IntegerLiteral*>(expr->rhs.get());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value() == 1);
}

TEST_CASE("Parse do-while statement", "[parse]") {
    std::string const text = R"(
fn test() {
    do {
        x += 1;
    } while x < 0;
})";
    auto const [ast, iss]  = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = cast<FunctionDefinition*>(tu->declarations[0].get());
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body.get();
    REQUIRE(body);
    REQUIRE(body->statements.size() == 1);
    auto* const doWhileStatement = cast<DoWhileStatement*>(body->statements[0].get());
    REQUIRE(doWhileStatement);
    auto* const condition = cast<BinaryExpression*>(doWhileStatement->condition.get());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement = cast<ExpressionStatement*>(doWhileStatement->block->statements[0].get());
    REQUIRE(exprStatement);
    auto* const expr = cast<BinaryExpression*>(exprStatement->expression.get());
    REQUIRE(expr);
    CHECK(expr->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier*>(expr->lhs.get());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<IntegerLiteral*>(expr->rhs.get());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value() == 1);
}

TEST_CASE("Parse for statement", "[parse]") {
    std::string const text = R"(
fn test() {
    for x = 0; x < 10; x += 1 {
        print(x);
    }
})";
    auto const [ast, iss]  = test::parse(text);
    REQUIRE(iss.empty());
    auto* const tu = cast<TranslationUnit*>(ast.get());
    REQUIRE(tu->declarations.size() == 1);
    auto* const function = cast<FunctionDefinition*>(tu->declarations[0].get());
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body.get();
    REQUIRE(body);
    REQUIRE(body->statements.size() == 1);
    auto* const forStatement = cast<ForStatement*>(body->statements[0].get());
    REQUIRE(forStatement);
    auto* const varDecl = cast<VariableDeclaration*>(forStatement->varDecl.get());
    REQUIRE(varDecl);
    CHECK(varDecl->name() == "x");
    CHECK(varDecl->typeExpr == nullptr);
    auto* const varInitExpr = cast<IntegerLiteral*>(varDecl->initExpression.get());
    REQUIRE(varInitExpr);
    CHECK(varInitExpr->value() == 0);
    auto* const condition = cast<BinaryExpression*>(forStatement->condition.get());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const increment = cast<BinaryExpression*>(forStatement->increment.get());
    REQUIRE(increment);
    CHECK(increment->operation() == BinaryOperator::AddAssignment);
    auto* const identifier = cast<Identifier*>(increment->lhs.get());
    REQUIRE(identifier);
    CHECK(identifier->value() == "x");
    auto* const intLiteral = cast<IntegerLiteral*>(increment->rhs.get());
    REQUIRE(intLiteral);
    CHECK(intLiteral->value() == 1);
    auto* const loopStatement = cast<ExpressionStatement*>(forStatement->block->statements[0].get());
    REQUIRE(loopStatement);
    auto* const functionCall = cast<FunctionCall*>(loopStatement->expression.get());
    REQUIRE(functionCall);
    CHECK(cast<Identifier*>(functionCall->object.get())->value() == "print");
}
