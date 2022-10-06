#include "Parser/Parser.h"

#include "Basic/Basic.h"
#include "Common/Keyword.h"
#include "Parser/ExpressionParser.h"
#include "Parser/ParsingIssue.h"

namespace scatha::parse {

/// MARK: Public
Parser::Parser(std::span<Token const> tokens): tokens(tokens) {}

ast::UniquePtr<ast::AbstractSyntaxTree> Parser::parse() { return parseTranslationUnit(); }

ast::UniquePtr<ast::TranslationUnit>    Parser::parseTranslationUnit() {
       ast::UniquePtr<ast::TranslationUnit> result = ast::allocate<ast::TranslationUnit>();

       while (true) {
           if (tokens.peek().type == TokenType::EndOfFile) {
               break;
        }
           auto decl = parseDeclaration();
           if (decl == nullptr) {
               throw ParsingIssue(tokens.peek(), "Expected Declarator");
        }
           result->declarations.push_back(std::move(decl));
    }

       return result;
}

/// MARK: Private
ast::UniquePtr<ast::Declaration> Parser::parseDeclaration() {
    TokenEx const &token = tokens.peek();

    SC_ASSERT(token.type != TokenType::EndOfFile, "Not our job");

    if (!token.isKeyword) {
        return nullptr;
    }

    switch (token.keyword) {
        using enum Keyword;
    case Var:
        [[fallthrough]];
    case Let:
        return parseVariableDeclaration();

    case Function:
        return parseFunctionDefinition();

    case Struct:
        return parseStructDefinition();

    default:
        break;
    }
    return nullptr;
}

ast::UniquePtr<ast::VariableDeclaration> Parser::parseVariableDeclaration(bool isFunctionParameter) {
    bool isConst = false;
    if (!isFunctionParameter) {
        auto const &decl = tokens.eat();
        SC_ASSERT_AUDIT(decl.isKeyword, "We should have checked this outside of this function");
        SC_ASSERT_AUDIT(decl.keyword == Keyword::Var || decl.keyword == Keyword::Let, "Same here");
        isConst = decl.keyword == Keyword::Let;
    }

    TokenEx const &name = tokens.eat();
    expectIdentifier(name);

    auto result                 = ast::allocate<ast::VariableDeclaration>(name);
    result->isConstant          = isConst;
    result->isFunctionParameter = isFunctionParameter;

    if (tokens.peek().id == ":") {
        tokens.eat();
        result->typeExpr = parsePostfixExpression();
    } else if (isFunctionParameter) {
        // guaranteed to throw
        expectID(tokens.peek(), ":");
    }

    if (tokens.peek().id == "=") {
        if (isFunctionParameter) {
            throw ParsingIssue(tokens.peek(), "Unqualified ID");
        }
        tokens.eat();
        result->initExpression = parseExpression();
    }

    if (!isFunctionParameter) {
        TokenEx const &next = tokens.eat(false);
        expectSeparator(next);
    }

    return result;
}

ast::UniquePtr<ast::FunctionDefinition> Parser::parseFunctionDefinition() {
    TokenEx const &declarator = tokens.eat();
    SC_EXPECT(declarator.isKeyword && declarator.keyword == Keyword::Function, "Should have checked this before");
    TokenEx const &name = tokens.eat();
    expectIdentifier(name);
    auto result = ast::allocate<ast::FunctionDefinition>(name);
    parseFunctionParameters(result.get());
    if (TokenEx const &token = tokens.peek(); token.id == "->") {
        tokens.eat();
        result->returnTypeExpr = parsePostfixExpression();
    } else {

        auto copy              = token;
        copy.id                = "void";
        copy.type              = TokenType::Identifier;
        result->returnTypeExpr = ast::allocate<ast::Identifier>(copy);
    }
    if (tokens.peek().id != "{") {
        throw ParsingIssue(tokens.peek(), "Expected '{' after function declaration");
    }
    result->body = parseBlock();
    return result;
}

void Parser::parseFunctionParameters(ast::FunctionDefinition *fn) {
    TokenEx const &openParan = tokens.eat();
    expectID(openParan, "(");
    if (tokens.peek().id == ")") {
        tokens.eat();
        return;
    }
    while (true) {
        fn->parameters.push_back(parseVariableDeclaration(/* isFunctionParameter = */ true));
        if (TokenEx const &next = tokens.eat(); next.id != ",") {
            expectID(next, ")");
            break;
        }
    }
}

ast::UniquePtr<ast::StructDefinition> Parser::parseStructDefinition() {
    TokenEx const &declarator = tokens.eat();
    SC_EXPECT(declarator.isKeyword && declarator.keyword == Keyword::Struct, "Should have checked this before");
    TokenEx const &name = tokens.eat();
    expectIdentifier(name);
    auto result = ast::allocate<ast::StructDefinition>(name);
    if (tokens.peek().id != "{") {
        throw ParsingIssue(tokens.peek(), "Expected '{' after struct declaration");
    }
    result->body = parseBlock();
    return result;
}

ast::UniquePtr<ast::Block> Parser::parseBlock() {
    auto const &openBrace = tokens.eat();
    expectID(openBrace, "{");

    auto result = ast::allocate<ast::Block>(openBrace);

    while (true) {
        if (tokens.peek().id == "}") {
            tokens.eat();
            break;
        }
        result->statements.push_back(parseStatement());
    }

    return result;
}

ast::UniquePtr<ast::Statement> Parser::parseStatement() {
    if (ast::UniquePtr<ast::Statement> result = parseDeclaration(); result != nullptr) {
        return result;
    }
    TokenEx const &token = tokens.peek();
    if (token.isControlFlow) {
        using enum Keyword;
        tokens.eat();
        switch (token.keyword) {
        case Return:
            return parseReturnStatement();
        case If:
            return parseIfStatement();
        case While:
            return parseWhileStatement();
        default:
            // Var / Let should have been handled above
            throw ParsingIssue(token, "Unexpected ID");
        }
    } else if (token.id == "{") {
        return parseBlock();
    } else {
        // We have not eaten the first token yet. Parsing an expression should
        // be fine.
        auto           result = ast::allocate<ast::ExpressionStatement>(parseExpression(), token);
        TokenEx const &next   = tokens.eat(false);
        expectSeparator(next);
        return result;
    }
}

ast::UniquePtr<ast::ReturnStatement> Parser::parseReturnStatement() {
    auto const &token = tokens.current();
    SC_ASSERT_AUDIT(token.id == "return", "");
    auto           result = ast::allocate<ast::ReturnStatement>(parseExpression(), token);
    TokenEx const &next   = tokens.eat(false);
    expectSeparator(next);
    return result;
}

ast::UniquePtr<ast::IfStatement> Parser::parseIfStatement() {
    auto const &keyword = tokens.current();
    SC_ASSERT_AUDIT(keyword.isKeyword, "");
    SC_ASSERT_AUDIT(keyword.keyword == Keyword::If, "");

    auto condition   = parseExpression();
    auto result      = ast::allocate<ast::IfStatement>(std::move(condition), keyword);

    result->ifBlock  = parseBlock();
    auto const &next = tokens.peek();
    if (!next.isKeyword || next.keyword != Keyword::Else) {
        return result;
    }
    tokens.eat();

    auto const &elseBlockBegin = tokens.peek();
    if (elseBlockBegin.id == "if") {
        tokens.eat();
        result->elseBlock = parseIfStatement();
    } else {
        result->elseBlock = parseBlock();
    }

    return result;
}

ast::UniquePtr<ast::WhileStatement> Parser::parseWhileStatement() {
    auto const &keyword = tokens.current();
    SC_ASSERT(keyword.isKeyword, "");
    SC_ASSERT(keyword.keyword == Keyword::While, "");

    auto condition = parseExpression();
    auto result    = ast::allocate<ast::WhileStatement>(std::move(condition), keyword);
    result->block  = parseBlock();
    return result;
}

ast::UniquePtr<ast::Expression> Parser::parseExpression() {
    ExpressionParser parser(tokens);
    return parser.parseExpression();
}

ast::UniquePtr<ast::Expression> Parser::parsePostfixExpression() {
    ExpressionParser parser(tokens);
    return parser.parsePostfix();
}

} // namespace scatha::parse
