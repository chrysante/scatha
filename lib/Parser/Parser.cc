#include "Parser/Parser.h"

#include "Basic/Basic.h"
#include "Common/Keyword.h"
#include "Common/Expected.h"
#include "Parser/ExpressionParser.h"
#include "Parser/ParsingIssue.h"
#include "Parser/TokenStream.h"

using namespace scatha;
using namespace parse;

using enum ParsingIssue::Reason;

namespace {

struct Context {
    ast::UniquePtr<ast::AbstractSyntaxTree> run();

    ast::UniquePtr<ast::TranslationUnit> parseTranslationUnit();
    ast::UniquePtr<ast::Declaration> parseDeclaration();
    ast::UniquePtr<ast::VariableDeclaration> parseVariableDeclaration(bool isFunctionParameter = false);
    ast::UniquePtr<ast::FunctionDefinition> parseFunctionDefinition();
    ast::UniquePtr<ast::StructDefinition> parseStructDefinition();
    ast::UniquePtr<ast::Block> parseBlock();
    ast::UniquePtr<ast::Statement> parseStatement();
    ast::UniquePtr<ast::ReturnStatement> parseReturnStatement();
    ast::UniquePtr<ast::IfStatement> parseIfStatement();
    ast::UniquePtr<ast::WhileStatement> parseWhileStatement();
    
    ast::UniquePtr<ast::Expression> parseExpression();
    ast::UniquePtr<ast::Expression> parsePostfixExpression();
    ast::UniquePtr<ast::Expression> parseConditionalExpression();

    /// Helpers
    
    void parseFunctionParameters(ast::FunctionDefinition*);
    
    TokenStream tokens;
    issue::ParsingIssueHandler& iss;
};

}

ast::UniquePtr<ast::AbstractSyntaxTree> parse::parse(utl::vector<Token> tokens,
                                                     issue::ParsingIssueHandler& iss)
{
    Context ctx{ .tokens{ std::move(tokens) }, .iss = iss };
    return ctx.run();
}

ast::UniquePtr<ast::AbstractSyntaxTree> Context::run() {
    return parseTranslationUnit();
}

ast::UniquePtr<ast::TranslationUnit> Context::parseTranslationUnit() {
    ast::UniquePtr<ast::TranslationUnit> result = ast::allocate<ast::TranslationUnit>();
    while (true) {
        Token const& token = tokens.peek();
        if (token.type == TokenType::EndOfFile) {
            break;
        }
        auto decl = parseDeclaration();
        if (decl != nullptr) {
            result->declarations.push_back(std::move(decl));
        }
    }
    return result;
}

ast::UniquePtr<ast::Declaration> Context::parseDeclaration() {
    Token const& token = tokens.peek();
    SC_ASSERT(token.type != TokenType::EndOfFile, "This should be handled by parseTranslationUnit()");
    if (!token.isDeclarator) {
        return nullptr;
    }
    switch (token.keyword) {
        using enum Keyword;
    case Var: [[fallthrough]];
    case Let: return parseVariableDeclaration();
    case Function: return parseFunctionDefinition();
    case Struct: return parseStructDefinition();
    default: break;
    }
    SC_UNREACHABLE();
//    iss.push(ParsingIssue(token, ExpectedDeclarator));
//    return nullptr;
}

ast::UniquePtr<ast::VariableDeclaration> Context::parseVariableDeclaration(bool isFunctionParameter) {
    bool isConst = false;
    if (!isFunctionParameter) {
        auto const& decl = tokens.eat();
        SC_ASSERT_AUDIT(decl.isKeyword, "We should have checked this outside of this function");
        SC_ASSERT_AUDIT(decl.keyword == Keyword::Var || decl.keyword == Keyword::Let, "Same here");
        isConst = decl.keyword == Keyword::Let;
    }
    Token const& name = tokens.eat();
    expectIdentifier(iss, name);
    auto result                 = ast::allocate<ast::VariableDeclaration>(name);
    result->isConstant          = isConst;
    result->isFunctionParameter = isFunctionParameter;
    if (tokens.peek().id == ":") {
        tokens.eat();
        result->typeExpr = parseConditionalExpression();
    }
    else if (isFunctionParameter) {
        iss.push(ParsingIssue::expectedID(tokens.peek(), ":"));
        return result;
    }
    if (tokens.peek().id == "=") {
        if (isFunctionParameter) {
            iss.push(ParsingIssue(tokens.peek(), UnqualifiedID));
            return result;
        }
        tokens.eat();
        result->initExpression = parseExpression();
//        if (!result->initExpression) {
//        }
    }
    if (!isFunctionParameter) {
        Token const& next = tokens.peek();
        if (expectSeparator(iss, next)) {
            tokens.eat();
        }
        else {
            tokens.advanceUntilStable();
        }
    }
    return result;
}

ast::UniquePtr<ast::FunctionDefinition> Context::parseFunctionDefinition() {
    Token const& declarator = tokens.eat();
    SC_EXPECT(declarator.isKeyword && declarator.keyword == Keyword::Function, "Should have checked this before");
    Token const& name = tokens.eat();
    expectIdentifier(iss, name);
    auto result = ast::allocate<ast::FunctionDefinition>(name);
    parseFunctionParameters(result.get());
    if (Token const& token = tokens.peek(); token.id == "->") {
        tokens.eat();
        result->returnTypeExpr = parsePostfixExpression();
    }
    else {
        auto copy              = token;
        copy.id                = "void";
        copy.type              = TokenType::Identifier;
        result->returnTypeExpr = ast::allocate<ast::Identifier>(copy);
    }
    if (!expectID(iss, tokens.peek(), "{")) {
        if (!tokens.advanceTo("{")) {
            return nullptr;
        }
    }
    result->body = parseBlock();
    return result;
}

void Context::parseFunctionParameters(ast::FunctionDefinition* fn) {
    Token const& openParan = tokens.eat();
    expectID(iss, openParan, "(");
    if (tokens.peek().id == ")") {
        tokens.eat();
        return;
    }
    while (true) {
        fn->parameters.push_back(parseVariableDeclaration(/* isFunctionParameter = */ true));
        if (Token const& next = tokens.eat(); next.id != ",") {
            expectID(iss, next, ")");
            break;
        }
    }
}

ast::UniquePtr<ast::StructDefinition> Context::parseStructDefinition() {
    Token const& declarator = tokens.eat();
    SC_EXPECT(declarator.isKeyword && declarator.keyword == Keyword::Struct, "Should have checked this before");
    Token const& name = tokens.eat();
    expectIdentifier(iss, name);
    auto result = ast::allocate<ast::StructDefinition>(name);
    if (!expectID(iss, tokens.peek(), "{")) {
        if (!tokens.advanceTo("{")) {
            return nullptr;
        }
    }
    result->body = parseBlock();
    return result;
}

ast::UniquePtr<ast::Block> Context::parseBlock() {
    auto const& openBrace = tokens.eat();
    expectID(iss, openBrace, "{");
    auto result = ast::allocate<ast::Block>(openBrace);
    while (true) {
        Token const& next = tokens.peek();
        if (next.id == "}") {
            tokens.eat();
            break;
        }
        auto statement = parseStatement();
        if (statement != nullptr) {
            result->statements.push_back(std::move(statement));
        }
//        tokens.advancePastSeparator();
    }
    return result;
}

ast::UniquePtr<ast::Statement> Context::parseStatement() {
    if (ast::UniquePtr<ast::Statement> result = parseDeclaration(); result != nullptr) {
        return result;
    }
    Token const& token = tokens.peek();
    if (token.isControlFlow) {
        using enum Keyword;
        tokens.eat();
        switch (token.keyword) {
        case Return: return parseReturnStatement();
        case If: return parseIfStatement();
        case While: return parseWhileStatement();
        default: {
            iss.push(ParsingIssue(token, UnqualifiedID));
            return nullptr;
        }
        }
    }
    else if (token.id == "{") {
        return parseBlock();
    }
    else {
        // We have not eaten the first token yet. Parsing an expression should
        // be fine.
        auto result       = ast::allocate<ast::ExpressionStatement>(parseExpression(), token);
        Token const& next = tokens.eat();
        expectSeparator(iss, next);
        return result;
    }
}

ast::UniquePtr<ast::ReturnStatement> Context::parseReturnStatement() {
    auto const& token = tokens.current();
    SC_ASSERT_AUDIT(token.id == "return", "");
    auto result       = ast::allocate<ast::ReturnStatement>(parseExpression(), token);
    Token const& next = tokens.eat();
    expectSeparator(iss, next);
    return result;
}

ast::UniquePtr<ast::IfStatement> Context::parseIfStatement() {
    auto const& keyword = tokens.current();
    SC_ASSERT_AUDIT(keyword.isKeyword, "");
    SC_ASSERT_AUDIT(keyword.keyword == Keyword::If, "");
    auto condition   = parseExpression();
    auto result      = ast::allocate<ast::IfStatement>(std::move(condition), keyword);
    result->ifBlock  = parseBlock();
    auto const& next = tokens.peek();
    if (!next.isKeyword || next.keyword != Keyword::Else) {
        return result;
    }
    tokens.eat();
    auto const& elseBlockBegin = tokens.peek();
    if (elseBlockBegin.id == "if") {
        tokens.eat();
        result->elseBlock = parseIfStatement();
    }
    else {
        result->elseBlock = parseBlock();
    }
    return result;
}

ast::UniquePtr<ast::WhileStatement> Context::parseWhileStatement() {
    auto const& keyword = tokens.current();
    SC_ASSERT(keyword.isKeyword, "");
    SC_ASSERT(keyword.keyword == Keyword::While, "");
    auto condition = parseExpression();
    auto result    = ast::allocate<ast::WhileStatement>(std::move(condition), keyword);
    result->block  = parseBlock();
    return result;
}

ast::UniquePtr<ast::Expression> Context::parseExpression() {
    ExpressionParser parser(tokens, iss);
    return parser.parseExpression();
}

ast::UniquePtr<ast::Expression> Context::parsePostfixExpression() {
    ExpressionParser parser(tokens, iss);
    return parser.parsePostfix();
}

ast::UniquePtr<ast::Expression> Context::parseConditionalExpression() {
    ExpressionParser parser(tokens, iss);
    return parser.parseConditional();
}
