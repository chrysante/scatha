#include "Parser/Parser.h"

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "Common/Keyword.h"
#include "Parser/Panic.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/TokenStream.h"

#include "Parser/ParserImpl.h"

using namespace scatha;
using namespace parse;

using enum SyntaxIssue::Reason;

ast::UniquePtr<ast::AbstractSyntaxTree> parse::parse(utl::vector<Token> tokens, issue::SyntaxIssueHandler& iss) {
    Context ctx{ .tokens{ std::move(tokens) }, .iss = iss };
    return ctx.run();
}

ast::UniquePtr<ast::AbstractSyntaxTree> Context::run() {
    return parseTranslationUnit();
}

// MARK: -  RDP

ast::UniquePtr<ast::TranslationUnit> Context::parseTranslationUnit() {
    ast::UniquePtr<ast::TranslationUnit> result = ast::allocate<ast::TranslationUnit>();
    while (true) {
        Token const token = tokens.peek();
        if (token.type == TokenType::EndOfFile) {
            break;
        }
        auto decl = parseExternalDeclaration();
        if (!decl) {
            iss.push(SyntaxIssue(tokens.peek(), ExpectedDeclarator));
            panic(tokens);
            continue;
        }
        result->declarations.push_back(std::move(decl));
    }
    return result;
}

ast::UniquePtr<ast::Declaration> Context::parseExternalDeclaration() {
    if (auto funcDef = parseFunctionDefinition()) {
        return funcDef;
    }
    if (auto structDef = parseStructDefinition()) {
        return structDef;
    }
    return nullptr;
}

template <utl::invocable_r<bool, Token const&>... Cond, std::predicate... F>
bool Context::recover(std::pair<Cond, F>... retry) {
    /// Eat a few tokens and see if we can find a list.
    /// 5 is pretty arbitrary here.
    int const maxDiscardedTokens = 5;
    for (int i = 0; i < maxDiscardedTokens; ++i) {
        Token const& next = tokens.peek();
        bool success = false;
        ([&](auto const& cond, auto const& callback) {
            if (std::invoke(cond, next)) {
                success = std::invoke(callback);
                return true;
            }
            return false;
        }(retry.first, retry.second) || ...);
        if (success) {
            return true;
        }
        tokens.eat();
    }
    /// Here we assume that \p panic() gets us past the function body.
    panic(tokens);
    return false;
}

template <std::predicate... F>
bool Context::recover(std::pair<std::string_view, F>... retry) {
    return recover(std::pair{ [&](Token const& token){ return token.id == retry.first; }, retry.second }...);
}

ast::UniquePtr<ast::FunctionDefinition> Context::parseFunctionDefinition() {
    Token const declarator = tokens.peek();
    if (declarator.keyword != Keyword::Function) {
        return nullptr;
    }
    tokens.eat();

    auto identifier = parseIdentifier();
    if (!identifier) {
        iss.push(SyntaxIssue(tokens.peek(), ExpectedIdentifier));
        bool const recovered = recover(std::pair{
            [&](Token const&) { return true; },
            [&]{ return bool(identifier = parseIdentifier()); }
        });
        if (!recovered) {
            return nullptr;
        }
    }
    auto result = ast::allocate<ast::FunctionDefinition>(declarator, std::move(identifier));
    /// Parse parameters
    using ParamListType = decltype(result->parameters);
    auto parseList      = [this] {
        return this->parseList<ParamListType>("(", ")", ",", [this] { return parseParameterDeclaration(); });
    };
    auto params = parseList();
    if (!params) {
        iss.push(SyntaxIssue::expectedID(tokens.peek(), "("));
        bool const recovered = recover(std::pair(std::string_view("{"),
                                                 [&]{
                                                     /// User forgot to specify parameter list?
                                                     /// Go on with parsing the body.
                                                     params = ParamListType{};
                                                     return true;
                                                 }),
                                       std::pair(std::string_view("("),
                                                 [&]{
                                                     /// Try parsing parameter list again.
                                                     return bool(params = parseList());
                                                 }));
        if (!recovered) {
            return nullptr;
        }
    }
    result->parameters = std::move(*params);
    if (Token const arrow = tokens.peek(); arrow.id == "->") {
        tokens.eat();
        result->returnTypeExpr = parseTypeExpression();
        if (!result->returnTypeExpr) {
            pushExpectedExpression(tokens.peek());
        }
    }
    auto body = parseCompoundStatement();
    if (!body) {
        iss.push(SyntaxIssue::expectedID(tokens.peek(), "{"));
        /// Eat a few tokens and see if we can find a compound statement.
        /// 5 is pretty arbitrary here.
        bool recovered = recover(std::pair(std::string_view(";"),
                                           [&] { return true; }),
                                 std::pair(std::string_view("{"),
                                           [&] { return bool(body = parseCompoundStatement()); }));
        if (!recovered) {
            return nullptr;
        }
    }
    result->body = std::move(body);
    return result;
}

ast::UniquePtr<ast::ParameterDeclaration> Context::parseParameterDeclaration() {
    Token const& idToken = tokens.peek();
    auto identifier = parseIdentifier();
    if (!identifier) {
        iss.push(SyntaxIssue(idToken, ExpectedIdentifier));
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.id == ":") {
                break;
            }
            if (next.id == "," || next.id == ")") {
                return nullptr;
            }
            if (next.type == TokenType::EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    Token const colon = tokens.peek();
    if (colon.id != ":") {
        iss.push(SyntaxIssue::expectedID(colon, ":"));
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.id == "," || next.id == ")") {
                return nullptr;
            }
            if (next.type == TokenType::EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    else {
        tokens.eat();
    }
    auto typeExpr = parseTypeExpression();
    if (!typeExpr) {
        pushExpectedExpression(tokens.peek());
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.id == "," || next.id == ")") {
                return nullptr;
            }
            if (next.type == TokenType::EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    return ast::allocate<ast::ParameterDeclaration>(std::move(identifier), std::move(typeExpr));
}

ast::UniquePtr<ast::StructDefinition> Context::parseStructDefinition() {
    Token const declarator = tokens.peek();
    if (declarator.keyword != Keyword::Struct) {
        return nullptr;
    }
    tokens.eat();
    auto identifier = parseIdentifier();
    if (!identifier) {
        iss.push(SyntaxIssue(tokens.peek(), ExpectedIdentifier));
        bool const recovered = recover(std::pair(std::string_view("{"), []{ return true; }));
        if (!recovered) {
            return nullptr;
        }
    }
    auto body = parseCompoundStatement();
    if (!body) {
        Token const curr = tokens.current();
        Token const next = tokens.peek();
        SC_DEBUGFAIL(); // Handle issue
    }
    return ast::allocate<ast::StructDefinition>(declarator, std::move(identifier), std::move(body));
}

ast::UniquePtr<ast::VariableDeclaration> Context::parseVariableDeclaration() {
    Token const declarator = tokens.peek();
    if (declarator.keyword != Keyword::Var && declarator.keyword != Keyword::Let) {
        return nullptr;
    }
    tokens.eat();

    auto identifier = parseIdentifier();
    if (!identifier) {
        Token const curr = tokens.current();
        Token const next = tokens.peek();
        SC_DEBUGFAIL(); // Handle issue
    }
    auto result = ast::allocate<ast::VariableDeclaration>(declarator, std::move(identifier));
    if (Token const colon = tokens.peek(); colon.id == ":") {
        tokens.eat();
        auto typeExpr = parseTypeExpression();
        if (!typeExpr) {
            Token const curr = tokens.current();
            Token const next = tokens.peek();
            SC_DEBUGFAIL(); // Handle issue
        }
        result->typeExpr = std::move(typeExpr);
    }
    if (Token const assign = tokens.peek(); assign.id == "=") {
        tokens.eat();
        auto initExpr = parseAssignment();
        if (!initExpr) {
            Token const curr = tokens.current();
            Token const next = tokens.peek();
            SC_DEBUGFAIL(); // Handle issue
        }
        result->initExpression = std::move(initExpr);
    }

    if (Token const semicolon = tokens.peek(); semicolon.id != ";") {
        Token const curr = tokens.current();
        Token const next = tokens.peek();
        SC_DEBUGFAIL(); // Handle issue
    }
    else {
        tokens.eat();
    }
    return result;
}

ast::UniquePtr<ast::Statement> Context::parseStatement() {
    if (auto extDecl = parseExternalDeclaration()) {
        return extDecl;
    }
    if (auto varDecl = parseVariableDeclaration()) {
        return varDecl;
    }
    if (auto controlFlowStatement = parseControlFlowStatement()) {
        return controlFlowStatement;
    }
    if (auto block = parseCompoundStatement()) {
        return block;
    }
    if (auto expressionStatement = parseExpressionStatement()) {
        return expressionStatement;
    }
    if (auto emptyStatement = parseEmptyStatement()) {
        return emptyStatement;
    }
    return nullptr;
}

ast::UniquePtr<ast::ExpressionStatement> Context::parseExpressionStatement() {
    auto expression = parseComma();
    if (!expression) {
        return nullptr;
    }
    expectDelimiter(";");
    return ast::allocate<ast::ExpressionStatement>(std::move(expression));
}

ast::UniquePtr<ast::CompoundStatement> Context::parseCompoundStatement() {
    Token const openBrace = tokens.peek();
    if (openBrace.id != "{") {
        return nullptr;
    }
    tokens.eat();
    auto result = ast::allocate<ast::CompoundStatement>(openBrace);
    while (true) {
        /// This mechanism checks wether a failed statement parse has eaten any tokens. If it hasn't we eat one
        /// ourselves and try again.
        auto const lastIndex = tokens.index();
        Token const next     = tokens.peek();
        if (next.id == "}") {
            tokens.eat();
            return result;
        }
        if (auto statement = parseStatement()) {
            result->statements.push_back(std::move(statement));
            continue;
        }
        if (tokens.index() == lastIndex) {
            /// If we can't parse a statement, eat one token and try again.
            iss.push(tokens.eat(), UnqualifiedID);
        }
    }
}

ast::UniquePtr<ast::ControlFlowStatement> Context::parseControlFlowStatement() {
    if (auto returnStatement = parseReturnStatement()) {
        return returnStatement;
    }
    if (auto ifStatement = parseIfStatement()) {
        return ifStatement;
    }
    if (auto whileStatement = parseWhileStatement()) {
        return whileStatement;
    }
    // not necessarily an error
    return nullptr;
}

ast::UniquePtr<ast::ReturnStatement> Context::parseReturnStatement() {
    Token const returnToken = tokens.peek();
    if (returnToken.id != "return") {
        return nullptr;
    }
    tokens.eat();
    // May be null in case of void return statement
    auto expression = parseComma();

    expectDelimiter(";");
    return ast::allocate<ast::ReturnStatement>(returnToken, std::move(expression));
}

ast::UniquePtr<ast::IfStatement> Context::parseIfStatement() {
    Token const ifToken = tokens.peek();
    if (ifToken.id != "if") {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto ifBlock = parseCompoundStatement();
    if (!ifBlock) {
        iss.push(SyntaxIssue::expectedID(tokens.peek(), "{"));
        SC_DEBUGFAIL();
    }
    auto elseBlock = [&]() -> ast::UniquePtr<ast::Statement> {
        Token const elseToken = tokens.peek();
        if (elseToken.id != "else") {
            return nullptr;
        }
        tokens.eat();
        if (auto elseBlock = parseIfStatement()) {
            return elseBlock;
        }
        if (auto elseBlock = parseCompoundStatement()) {
            return elseBlock;
        }
        Token const curr = tokens.current();
        Token const next = tokens.peek();
        SC_DEBUGFAIL(); // Handle issue
        return nullptr;
    }();
    return ast::allocate<ast::IfStatement>(ifToken, std::move(cond), std::move(ifBlock), std::move(elseBlock));
}

ast::UniquePtr<ast::WhileStatement> Context::parseWhileStatement() {
    Token const whileToken = tokens.peek();
    if (whileToken.id != "while") {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto block = parseCompoundStatement();
    if (!block) {
        iss.push(SyntaxIssue::expectedID(tokens.peek(), "{"));
    }
    return ast::allocate<ast::WhileStatement>(whileToken, std::move(cond), std::move(block));
}

ast::UniquePtr<ast::EmptyStatement> Context::parseEmptyStatement() {
    Token const delim = tokens.peek();
    if (delim.id != ";") {
        return nullptr;
    }
    tokens.eat();
    return ast::allocate<ast::EmptyStatement>(delim);
}

// MARK: - Expressions

ast::UniquePtr<ast::Expression> Context::parseComma() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Comma>([this] { return parseAssignment(); });
}

ast::UniquePtr<ast::Expression> Context::parseAssignment() {
    using enum ast::BinaryOperator;
    return parseBinaryOperatorRTL<Assignment,
                                  AddAssignment,
                                  SubAssignment,
                                  MulAssignment,
                                  DivAssignment,
                                  RemAssignment,
                                  LSAssignment,
                                  RSAssignment,
                                  AndAssignment,
                                  OrAssignment,
                                  XOrAssignment>([this] { return parseConditional(); });
}

ast::UniquePtr<ast::Expression> Context::parseConditional() {
    Token const& condToken = tokens.peek();
    auto logicalOr         = parseLogicalOr();
    if (auto const& questionMark = tokens.peek(); questionMark.id == "?") {
        if (!logicalOr) {
            pushExpectedExpression(condToken);
        }
        tokens.eat();
        auto lhs = parseComma();
        if (!lhs) {
            pushExpectedExpression(tokens.peek());
        }
        Token const& colon = tokens.peek();
        if (colon.id != ":") {
            iss.push(SyntaxIssue::expectedID(colon, ":"));
        }
        else {
            tokens.eat();
        }
        auto rhs = parseConditional();
        if (!rhs) {
            pushExpectedExpression(tokens.peek());
        }
        return allocate<ast::Conditional>(std::move(logicalOr), std::move(lhs), std::move(rhs), questionMark);
    }
    return logicalOr;
}

ast::UniquePtr<ast::Expression> Context::parseLogicalOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalOr>([this] { return parseLogicalAnd(); });
}

ast::UniquePtr<ast::Expression> Context::parseLogicalAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalAnd>([this] { return parseInclusiveOr(); });
}

ast::UniquePtr<ast::Expression> Context::parseInclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseOr>([this] { return parseExclusiveOr(); });
}

ast::UniquePtr<ast::Expression> Context::parseExclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseXOr>([this] { return parseAnd(); });
}

ast::UniquePtr<ast::Expression> Context::parseAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseAnd>([this] { return parseEquality(); });
}

ast::UniquePtr<ast::Expression> Context::parseEquality() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Equals, ast::BinaryOperator::NotEquals>(
        [this] { return parseRelational(); });
}

ast::UniquePtr<ast::Expression> Context::parseRelational() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Less,
                                  ast::BinaryOperator::LessEq,
                                  ast::BinaryOperator::Greater,
                                  ast::BinaryOperator::GreaterEq>([this] { return parseShift(); });
}

ast::UniquePtr<ast::Expression> Context::parseShift() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LeftShift, ast::BinaryOperator::RightShift>(
        [this] { return parseAdditive(); });
}

ast::UniquePtr<ast::Expression> Context::parseAdditive() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Addition, ast::BinaryOperator::Subtraction>(
        [this] { return parseMultiplicative(); });
}

ast::UniquePtr<ast::Expression> Context::parseMultiplicative() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Multiplication,
                                  ast::BinaryOperator::Division,
                                  ast::BinaryOperator::Remainder>([this] { return parseUnary(); });
}

ast::UniquePtr<ast::Expression> Context::parseUnary() {
    if (auto postfix = parsePostfix()) {
        return postfix;
    }
    Token const token = tokens.peek();
    auto makeResult   = [&](ast::UnaryPrefixOperator operatorType) {
        Token const& unaryToken = tokens.peek();
        auto unary              = parseUnary();
        if (!unary) {
            pushExpectedExpression(unaryToken);
        }
        return ast::allocate<ast::UnaryPrefixExpression>(operatorType, std::move(unary), token);
    };
    if (token.id == "&") {
        SC_DEBUGFAIL(); // Do we really want to support addressof operator?
    }
    else if (token.id == "+") {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Promotion);
    }
    else if (token.id == "-") {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Negation);
    }
    else if (token.id == "~") {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::BitwiseNot);
    }
    else if (token.id == "!") {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::LogicalNot);
    }
    else {
        return nullptr;
    }
}

ast::UniquePtr<ast::Expression> Context::parsePostfix() {
    auto primary = parsePrimary();
    if (!primary) {
        return nullptr;
    }
    while (true) {
        auto const& token = tokens.peek();
        if (token.id == "[") {
            primary = parseSubscript(std::move(primary));
        }
        else if (token.id == "(") {
            primary = parseFunctionCall(std::move(primary));
        }
        else if (token.id == ".") {
            primary = parseMemberAccess(std::move(primary));
        }
        else {
            break;
        }
    }
    return primary;
}

ast::UniquePtr<ast::Expression> Context::parsePrimary() {
    if (auto result = parseIdentifier()) {
        return result;
    }
    if (auto result = parseIntegerLiteral()) {
        return result;
    }
    if (auto result = parseBooleanLiteral()) {
        return result;
    }
    if (auto result = parseFloatingPointLiteral()) {
        return result;
    }
    if (auto result = parseStringLiteral()) {
        return result;
    }
    auto const& token = tokens.peek();
    if (token.id == "(") {
        tokens.eat();
        Token const& commaToken               = tokens.peek();
        ast::UniquePtr<ast::Expression> comma = parseComma();
        if (!comma) {
            pushExpectedExpression(commaToken);
        }
        expectDelimiter(")");
        return comma;
    }
    return nullptr;
}

ast::UniquePtr<ast::Identifier> Context::parseIdentifier() {
    Token const next = tokens.peek();
    if (next.type != TokenType::Identifier || next.isDeclarator) {
        return nullptr;
    }
    return ast::allocate<ast::Identifier>(tokens.eat());
}

ast::UniquePtr<ast::IntegerLiteral> Context::parseIntegerLiteral() {
    if (tokens.peek().type != TokenType::IntegerLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::IntegerLiteral>(tokens.eat());
}

ast::UniquePtr<ast::BooleanLiteral> Context::parseBooleanLiteral() {
    if (tokens.peek().type != TokenType::BooleanLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::BooleanLiteral>(tokens.eat());
}

ast::UniquePtr<ast::FloatingPointLiteral> Context::parseFloatingPointLiteral() {
    if (tokens.peek().type != TokenType::FloatingPointLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::FloatingPointLiteral>(tokens.eat());
}

ast::UniquePtr<ast::StringLiteral> Context::parseStringLiteral() {
    if (tokens.peek().type != TokenType::StringLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::StringLiteral>(tokens.eat());
}

template <typename FunctionCallLike>
ast::UniquePtr<FunctionCallLike> Context::parseFunctionCallLike(ast::UniquePtr<ast::Expression> primary,
                                                                std::string_view open,
                                                                std::string_view close) {
    auto const& openToken = tokens.peek();
    SC_ASSERT(openToken.id == open, "");
    auto result       = ast::allocate<FunctionCallLike>(std::move(primary), openToken);
    result->arguments = parseList<decltype(result->arguments)>(open, close, ",", [this] {
                            return parseAssignment();
                        }).value(); // handle error
    return result;
}

template <typename, typename List>
std::optional<List> Context::parseList(std::string_view open,
                                       std::string_view close,
                                       std::string_view delimiter,
                                       auto parseCallback) {
    auto const& openToken = tokens.peek();
    if (openToken.id != open) {
        return std::nullopt;
    }
    tokens.eat();
    List result;
    bool first = true;
    while (true) {
        Token const next = tokens.peek();
        if (next.id == close) {
            tokens.eat();
            return result;
        }
        else if (!first) {
            expectDelimiter(delimiter);
        }
        first     = false;
        auto elem = parseCallback();
        if (elem) {
            result.push_back(std::move(elem));
        }
        else {
#warning Maybe delegate error handling to the parse callback to avoid duplicate errors in some cases
            iss.push(SyntaxIssue(tokens.peek(), ExpectedExpression));
            /// Without eating a token we may get stuck in an infinite loop, otherwise we may miss delimiters in case of
            /// syntax errors (especcially missing ')').
            //          tokens.eat();
        }
    }
    return result;
}

ast::UniquePtr<ast::Subscript> Context::parseSubscript(ast::UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::Subscript>(std::move(primary), "[", "]");
}

ast::UniquePtr<ast::FunctionCall> Context::parseFunctionCall(ast::UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::FunctionCall>(std::move(primary), "(", ")");
}

ast::UniquePtr<ast::Expression> Context::parseMemberAccess(ast::UniquePtr<ast::Expression> left) {
    SC_ASSERT(tokens.peek().id == ".", "");
    while (true) {
        auto const& token = tokens.peek();
        if (token.id != ".") {
            return left;
        }
        tokens.eat();
        ast::UniquePtr<ast::Expression> right = parseIdentifier();
        if (!right) {
            iss.push(SyntaxIssue(tokens.peek(), ExpectedExpression));
        }
        left = ast::allocate<ast::MemberAccess>(std::move(left), std::move(right), token);
        continue;
    }
}

void Context::pushExpectedExpression(Token const& token) {
    iss.push(token, ExpectedExpression);
}

template <ast::BinaryOperator... Op>
ast::UniquePtr<ast::Expression> Context::parseBinaryOperatorLTR(auto&& operand) {
    Token const& lhsToken                = tokens.peek();
    ast::UniquePtr<ast::Expression> left = operand();
    auto tryParse                        = [&](Token const token, ast::BinaryOperator op) {
        if (token.id != toString(op)) {
            return false;
        }
        tokens.eat();
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        Token const& rhsToken                 = tokens.peek();
        ast::UniquePtr<ast::Expression> right = operand();
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        left = ast::allocate<ast::BinaryExpression>(op, std::move(left), std::move(right), token);
        return true;
    };
    while (true) {
        Token const token = tokens.peek();
        if ((tryParse(token, Op) || ...)) {
            continue;
        }
        return left;
    }
}

template <ast::BinaryOperator... Op>
ast::UniquePtr<ast::Expression> Context::parseBinaryOperatorRTL(auto&& parseOperand) {
    Token const& lhsToken = tokens.peek();
    SC_ASSERT(lhsToken.type != TokenType::EndOfFile, "");
    ast::UniquePtr<ast::Expression> left = parseOperand();
    Token const operatorToken            = tokens.peek();
    auto parse                           = [&](ast::BinaryOperator op) {
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        tokens.eat();
        Token const& rhsToken                 = tokens.peek();
        ast::UniquePtr<ast::Expression> right = parseBinaryOperatorRTL<Op...>(parseOperand);
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        return ast::allocate<ast::BinaryExpression>(op, std::move(left), std::move(right), operatorToken);
    };
    if (ast::UniquePtr<ast::Expression> result = nullptr;
        ((operatorToken.id == toString(Op) && ((result = parse(Op)), true)) || ...))
    {
        return result;
    }
    return left;
}

void Context::expectDelimiter(std::string_view delimiter) {
    if (auto next = tokens.peek(); next.id != delimiter) {
        auto const curr = tokens.current();
        auto location   = curr.sourceLocation;
        location.index += curr.id.size();  //
        location.column += curr.id.size(); // Adding to column should be fine because no line wrapping in tokens
        iss.push(SyntaxIssue(location, ExpectedSeparator));
    }
    else {
        tokens.eat();
    }
}
