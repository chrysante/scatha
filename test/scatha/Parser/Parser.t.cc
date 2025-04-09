#include <catch2/catch_test_macros.hpp>

#include "AST/AST.h"
#include "Parser/SimpleParser.h"
#include "Util/IssueHelper.h"

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
    auto* const file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* const function = file->statement<FunctionDefinition>(0);
    CHECK(function->name() == "mul");
    REQUIRE(function->parameters().size() == 2);
    CHECK(function->parameters()[0]->name() == "a");
    auto const* aTypeExpr =
        cast<Identifier*>(function->parameters()[0]->typeExpr());
    CHECK(aTypeExpr->value() == "int");
    CHECK(function->parameters()[1]->name() == "b");
    auto const* bTypeExpr =
        cast<MemberAccess*>(function->parameter(1)->typeExpr());
    auto const* bTypeExprLhs =
        dyncast<MemberAccess const*>(bTypeExpr->accessed());
    REQUIRE(bTypeExprLhs);
    CHECK(cast<Identifier const*>(bTypeExprLhs->accessed())->value() == "X");
    CHECK(cast<Identifier const*>(bTypeExprLhs->member())->value() == "Y");
    CHECK(cast<Identifier const*>(bTypeExpr->member())->value() == "Z");
    auto const* returnTypeExpr = cast<Identifier*>(function->returnTypeExpr());
    CHECK(returnTypeExpr->value() == "int");
    CompoundStatement* const body = function->body();
    REQUIRE(body->statements().size() == 2);
    auto* const resultDecl = cast<VariableDeclaration*>(body->statement(0));
    CHECK(resultDecl->name() == "result");
    CHECK(!resultDecl->typeExpr());
    // CHECK(!resultDecl->isConstant);
    CHECK(isa<Identifier>(resultDecl->initExpr()));
    auto* const returnStatement =
        cast<ReturnStatement const*>(body->statement(1));
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
    auto* const file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* const function = file->statement<FunctionDefinition>(0);
    CHECK(function->name() == "main");
    auto* const aDecl =
        cast<VariableDeclaration*>(function->body()->statement(0));
    auto* const intLit = cast<Literal const*>(aDecl->initExpr());
    CHECK(intLit->value<APInt>() == 39);
    auto* const bDecl =
        cast<VariableDeclaration const*>(function->body()->statement(1));
    auto* const floatLit = cast<Literal const*>(bDecl->initExpr());
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
    auto* const file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* const function = file->statement<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const whileStatement = cast<LoopStatement const*>(body->statement(0));
    REQUIRE(whileStatement);
    auto* const condition =
        cast<BinaryExpression const*>(whileStatement->condition());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement =
        cast<ExpressionStatement const*>(whileStatement->block()->statement(0));
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
    auto* const file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* const function = file->statement<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const doWhileStatement =
        cast<LoopStatement const*>(body->statement(0));
    REQUIRE(doWhileStatement);
    auto* const condition =
        cast<BinaryExpression const*>(doWhileStatement->condition());
    REQUIRE(condition);
    CHECK(condition->operation() == BinaryOperator::Less);
    auto* const exprStatement = cast<ExpressionStatement const*>(
        doWhileStatement->block()->statement(0));
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
    auto* const file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* const function = file->statement<FunctionDefinition>(0);
    REQUIRE(function);
    CHECK(function->name() == "test");
    CompoundStatement* const body = function->body();
    REQUIRE(body);
    REQUIRE(body->statements().size() == 1);
    auto* const forStatement = cast<LoopStatement const*>(body->statement(0));
    REQUIRE(forStatement);
    auto* const varDecl =
        cast<VariableDeclaration const*>(forStatement->varDecl());
    REQUIRE(varDecl);
    CHECK(varDecl->name() == "x");
    CHECK(varDecl->typeExpr() == nullptr);
    auto* const varInitExpr = cast<Literal const*>(varDecl->initExpr());
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
    auto* const loopStatement =
        cast<ExpressionStatement const*>(forStatement->block()->statement(0));
    REQUIRE(loopStatement);
    auto* const functionCall =
        cast<FunctionCall const*>(loopStatement->expression());
    REQUIRE(functionCall);
    CHECK(cast<Identifier const*>(functionCall->callee())->value() == "print");
}

TEST_CASE("Parse fstrings", "[parse]") {
    std::string const text = R"TEXT(
    fn main() {
    "a \( xyz )";
    "a \( (9 + (7)) ) \(3)";
    "\("\("")\("")")";
})TEXT";
    auto [ast, iss] = test::parse(text);
    REQUIRE(iss.empty());
    auto* file = cast<TranslationUnit*>(ast.get())->sourceFile(0);
    REQUIRE(file->statements().size() == 1);
    auto* function = file->statement<FunctionDefinition>(0);
    auto* body = function->body();
    REQUIRE((body && body->statements().size() == 3));
    using enum ast::LiteralKind;
    {
        auto* expr =
            body->statement<ExpressionStatement>(0)->expression<FStringExpr>();
        REQUIRE(expr->operands().size() == 3);
        auto* begin = expr->operand<Literal>(0);
        CHECK(begin->kind() == FStringBegin);
        CHECK(begin->value<std::string>() == "a ");
        auto* ID = expr->operand<Identifier>(1);
        CHECK(ID->value() == "xyz");
        auto* end = expr->operand<Literal>(2);
        CHECK(end->kind() == FStringEnd);
        CHECK(end->value<std::string>() == "");
    }
    {
        auto* expr =
            body->statement<ExpressionStatement>(1)->expression<FStringExpr>();
        REQUIRE(expr->operands().size() == 5);
        auto* begin = expr->operand<Literal>(0);
        CHECK(begin->kind() == FStringBegin);
        CHECK(begin->value<std::string>() == "a ");
        CHECK(expr->operand<BinaryExpression>(1));
        auto* cont = expr->operand<Literal>(2);
        CHECK(cont->kind() == FStringContinue);
        CHECK(cont->value<std::string>() == " ");
        CHECK(expr->operand<Literal>(3));
        auto* end = expr->operand<Literal>(4);
        CHECK(end->kind() == FStringEnd);
        CHECK(end->value<std::string>() == "");
    }
    {
        auto* expr =
            body->statement<ExpressionStatement>(2)->expression<FStringExpr>();
        REQUIRE(expr->operands().size() == 3);
        auto* begin = expr->operand<Literal>(0);
        CHECK(begin->kind() == FStringBegin);
        CHECK(begin->value<std::string>() == "");
        {
            auto* nested = expr->operand<FStringExpr>(1);
            REQUIRE(nested->operands().size() == 5);
            auto* begin = nested->operand<Literal>(0);
            CHECK(begin->kind() == FStringBegin);
            CHECK(begin->value<std::string>() == "");
            CHECK(nested->operand<Literal>(1));
            auto* cont = nested->operand<Literal>(2);
            CHECK(cont->kind() == FStringContinue);
            CHECK(cont->value<std::string>() == "");
            CHECK(nested->operand<Literal>(3));
            auto* end = nested->operand<Literal>(4);
            CHECK(end->kind() == FStringEnd);
            CHECK(end->value<std::string>() == "");
        }
        auto* end = expr->operand<Literal>(2);
        CHECK(end->kind() == FStringEnd);
        CHECK(end->value<std::string>() == "");
    }
}
