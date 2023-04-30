#include "Parser/Parser.h"

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Expected.h"
#include "Parser/BracketCorrection.h"
#include "Parser/Lexer.h"
#include "Parser/Panic.h"
#include "Parser/SyntaxIssue.h"
#include "Parser/TokenStream.h"

#include "Parser/ParserImpl.h"

using namespace scatha;
using namespace parse;

using enum TokenKind;

UniquePtr<ast::AbstractSyntaxTree> parse::parse(utl::vector<Token> tokens,
                                                IssueHandler& issues) {
    bracketCorrection(tokens, issues);
    Context ctx{ .tokens{ std::move(tokens) }, .issues = issues };
    return ctx.run();
}

UniquePtr<ast::AbstractSyntaxTree> parse::parse(std::string_view source,
                                                IssueHandler& issueHandler) {
    auto tokens = lex(source, issueHandler);
    bracketCorrection(tokens, issueHandler);
    Context ctx{ .tokens{ std::move(tokens) }, .issues = issueHandler };
    return ctx.run();
}

UniquePtr<ast::AbstractSyntaxTree> Context::run() {
    return parseTranslationUnit();
}

// MARK: -  RDP

UniquePtr<ast::TranslationUnit> Context::parseTranslationUnit() {
    UniquePtr<ast::TranslationUnit> result = allocate<ast::TranslationUnit>();
    while (true) {
        Token const token = tokens.peek();
        if (token.kind() == EndOfFile) {
            break;
        }
        auto decl = parseExternalDeclaration();
        if (!decl) {
            issues.push<ExpectedDeclarator>(tokens.peek());
            panic(tokens);
            continue;
        }
        result->declarations.push_back(std::move(decl));
    }
    return result;
}

UniquePtr<ast::Declaration> Context::parseExternalDeclaration() {
    ast::AccessSpec accessSpec = ast::AccessSpec::None;
    if (isAccessSpec(tokens.peek().kind())) {
        Token const token = tokens.eat();
        if (token.kind() == Public) {
            accessSpec = ast::AccessSpec::Public;
        }
        if (token.kind() == Private) {
            accessSpec = ast::AccessSpec::Private;
        }
    }
    if (auto funcDef = parseFunctionDefinition()) {
        funcDef->accessSpec = accessSpec;
        return funcDef;
    }
    if (auto structDef = parseStructDefinition()) {
        structDef->accessSpec = accessSpec;
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
        bool success      = false;
        ([&](auto const& cond, auto const& callback) {
            if (std::invoke(cond, next)) {
                success = std::invoke(callback);
                return true;
            }
            return false;
        }(retry.first, retry.second) ||
         ...);
        if (success) {
            return true;
        }
        tokens.eat();
    }
    /// Here we assume that `panic()` gets us past the function body.
    panic(tokens);
    return false;
}

template <std::predicate... F>
bool Context::recover(std::pair<TokenKind, F>... retry) {
    return recover(std::pair{
        [&](Token const& token) { return token.kind() == retry.first; },
        retry.second }...);
}

UniquePtr<ast::FunctionDefinition> Context::parseFunctionDefinition() {
    Token const declarator = tokens.peek();
    if (declarator.kind() != Function) {
        return nullptr;
    }
    tokens.eat();
    auto identifier = parseIdentifier();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.peek());
        bool const recovered = recover(
            std::pair{ [&](Token const&) { return true; },
                       [&] { return bool(identifier = parseIdentifier()); } });
        if (!recovered) {
            return nullptr;
        }
    }
    /// Parse parameters
    using ParamListType =
        utl::small_vector<UniquePtr<ast::ParameterDeclaration>>;
    auto parseList = [this] {
        return this->parseList<ParamListType>(OpenParan,
                                              CloseParan,
                                              Comma,
                                              [this] {
            return parseParameterDeclaration();
        });
    };
    auto params = parseList();
    if (!params) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenParan);
        // clang-format off
        bool const recovered = recover(std::pair{ OpenBrace, [&] {
            /// User forgot to specify parameter list?
            /// Go on with parsing the body.
            params = ParamListType{};
            return true;
        } }, std::pair{ OpenParan, [&] {
            /// Try parsing parameter list again.
            return bool(params = parseList());
        }});
        // clang-format on
        if (!recovered) {
            return nullptr;
        }
    }
    UniquePtr<ast::Expression> returnTypeExpr;
    if (Token const arrow = tokens.peek(); arrow.kind() == Arrow) {
        tokens.eat();
        returnTypeExpr = parseTypeExpression();
        if (!returnTypeExpr) {
            pushExpectedExpression(tokens.peek());
        }
    }
    auto body = parseCompoundStatement();
    if (!body) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
        /// Eat a few tokens and see if we can find a compound statement.
        /// 5 is pretty arbitrary here.
        bool recovered =
            recover(std::pair(Semicolon, [&] { return true; }),
                    std::pair(OpenBrace, [&] {
                        return bool(body = parseCompoundStatement());
                    }));
        if (!recovered) {
            return nullptr;
        }
    }
    return allocate<ast::FunctionDefinition>(declarator.sourceRange(),
                                             std::move(identifier),
                                             std::move(*params),
                                             std::move(returnTypeExpr),
                                             std::move(body));
}

UniquePtr<ast::ParameterDeclaration> Context::parseParameterDeclaration() {
    Token const idToken = tokens.peek();
    if (idToken.kind() == This) {
        tokens.eat();
        return allocate<ast::ThisParameter>(idToken.sourceRange(),
                                            sema::TypeQualifiers::None);
    }
    if (idToken.kind() == BitAnd) {
        tokens.eat();
        sema::TypeQualifiers quals = sema::TypeQualifiers::ImplicitReference;
        if (eatMut()) {
            quals |= sema::TypeQualifiers::Mutable;
        }
        if (tokens.peek().kind() != This) {
            return nullptr;
        }
        auto const thisToken = tokens.eat();
        auto sourceRange =
            merge(idToken.sourceRange(), thisToken.sourceRange());
        return allocate<ast::ThisParameter>(sourceRange, quals);
    }
    auto identifier = parseIdentifier();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(idToken);
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.kind() == Colon) {
                break;
            }
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    Token const colon = tokens.peek();
    if (colon.kind() != Colon) {
        issues.push<UnqualifiedID>(colon, Colon);
        /// Custom recovery mechanism
        while (true) {
            Token const& next = tokens.peek();
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
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
            if (next.kind() == Comma || next.kind() == CloseParan) {
                return nullptr;
            }
            if (next.kind() == EndOfFile) {
                return nullptr;
            }
            tokens.eat();
        }
    }
    return allocate<ast::ParameterDeclaration>(std::move(identifier),
                                               std::move(typeExpr));
}

UniquePtr<ast::StructDefinition> Context::parseStructDefinition() {
    Token const declarator = tokens.peek();
    if (declarator.kind() != Struct) {
        return nullptr;
    }
    tokens.eat();
    auto identifier = parseIdentifier();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.peek());
        bool const recovered =
            recover(std::pair(OpenBrace, [] { return true; }));
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
    return allocate<ast::StructDefinition>(declarator.sourceRange(),
                                           std::move(identifier),
                                           std::move(body));
}

UniquePtr<ast::VariableDeclaration> Context::parseVariableDeclaration() {
    Token const declarator = tokens.peek();
    if (declarator.kind() != Var && declarator.kind() != Let) {
        return nullptr;
    }
    tokens.eat();
    return parseShortVariableDeclaration(declarator);
}

UniquePtr<ast::VariableDeclaration> Context::parseShortVariableDeclaration(
    std::optional<Token> declarator) {
    auto identifier = parseIdentifier();
    if (!identifier) {
        issues.push<ExpectedIdentifier>(tokens.current());
        panic(tokens);
    }
    auto result =
        allocate<ast::VariableDeclaration>(declarator ?
                                               declarator->sourceRange() :
                                           identifier ?
                                               identifier->sourceRange() :
                                               tokens.current().sourceRange(),
                                           std::move(identifier));
    if (Token const colon = tokens.peek(); colon.kind() == Colon) {
        tokens.eat();
        auto typeExpr = parseTypeExpression();
        if (!typeExpr) {
            issues.push<ExpectedExpression>(tokens.current());
            panic(tokens);
        }
        result->typeExpr = std::move(typeExpr);
    }
    if (Token const assign = tokens.peek(); assign.kind() == Assign) {
        tokens.eat();
        auto initExpr = parseAssignment();
        if (!initExpr) {
            issues.push<ExpectedExpression>(tokens.current());
            panic(tokens);
        }
        result->initExpression = std::move(initExpr);
    }
    if (Token const semicolon = tokens.peek(); semicolon.kind() != Semicolon) {
        issues.push<ExpectedDelimiter>(tokens.current());
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    return result;
}

UniquePtr<ast::Statement> Context::parseStatement() {
    if (auto extDecl = parseExternalDeclaration()) {
        return extDecl;
    }
    if (auto varDecl = parseVariableDeclaration()) {
        return varDecl;
    }
    if (auto controlFlowStatement = parseControlFlowStatement()) {
        return controlFlowStatement;
    }
    if (auto breakStatement = parseJumpStatement()) {
        return breakStatement;
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

UniquePtr<ast::ExpressionStatement> Context::parseExpressionStatement() {
    auto expression = parseComma();
    if (!expression) {
        return nullptr;
    }
    expectDelimiter(Semicolon);
    return allocate<ast::ExpressionStatement>(std::move(expression));
}

UniquePtr<ast::CompoundStatement> Context::parseCompoundStatement() {
    Token const openBrace = tokens.peek();
    if (openBrace.kind() != OpenBrace) {
        return nullptr;
    }
    tokens.eat();
    utl::small_vector<UniquePtr<ast::Statement>> statements;
    while (true) {
        /// This mechanism checks wether a failed statement parse has eaten any
        /// tokens. If it hasn't we eat one ourselves and try again.
        auto const lastIndex = tokens.index();
        Token const next     = tokens.peek();
        if (next.kind() == CloseBrace) {
            tokens.eat();
            break;
        }
        if (auto statement = parseStatement()) {
            statements.push_back(std::move(statement));
            continue;
        }
        if (tokens.index() == lastIndex) {
            /// If we can't parse a statement, eat one token and try again.
            issues.push<UnqualifiedID>(tokens.eat(), CloseBrace);
        }
    }
    return allocate<ast::CompoundStatement>(openBrace.sourceRange(),
                                            std::move(statements));
}

UniquePtr<ast::ControlFlowStatement> Context::parseControlFlowStatement() {
    if (auto returnStatement = parseReturnStatement()) {
        return returnStatement;
    }
    if (auto ifStatement = parseIfStatement()) {
        return ifStatement;
    }
    if (auto loop = parseWhileStatement()) {
        return loop;
    }
    if (auto loop = parseDoWhileStatement()) {
        return loop;
    }
    if (auto loop = parseForStatement()) {
        return loop;
    }
    return nullptr;
}

UniquePtr<ast::ReturnStatement> Context::parseReturnStatement() {
    Token const returnToken = tokens.peek();
    if (returnToken.kind() != Return) {
        return nullptr;
    }
    tokens.eat();
    /// May be null in case of void return statement.
    auto expression = parseComma();
    expectDelimiter(Semicolon);
    return allocate<ast::ReturnStatement>(returnToken.sourceRange(),
                                          std::move(expression));
}

UniquePtr<ast::IfStatement> Context::parseIfStatement() {
    Token const ifToken = tokens.peek();
    if (ifToken.kind() != If) {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto ifBlock = parseCompoundStatement();
    if (!ifBlock) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
        SC_DEBUGFAIL();
    }
    auto elseBlock = [&]() -> UniquePtr<ast::Statement> {
        Token const elseToken = tokens.peek();
        if (elseToken.kind() != Else) {
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
    return allocate<ast::IfStatement>(ifToken.sourceRange(),
                                      std::move(cond),
                                      std::move(ifBlock),
                                      std::move(elseBlock));
}

UniquePtr<ast::LoopStatement> Context::parseWhileStatement() {
    Token const whileToken = tokens.peek();
    if (whileToken.kind() != While) {
        return nullptr;
    }
    tokens.eat();
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    auto block = parseCompoundStatement();
    if (!block) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    return allocate<ast::LoopStatement>(whileToken.sourceRange(),
                                        ast::LoopKind::While,
                                        nullptr,
                                        std::move(cond),
                                        nullptr,
                                        std::move(block));
}

UniquePtr<ast::LoopStatement> Context::parseDoWhileStatement() {
    Token const doToken = tokens.peek();
    if (doToken.kind() != Do) {
        return nullptr;
    }
    tokens.eat();
    auto block = parseCompoundStatement();
    if (!block) {
        // TODO: Push a better issue here
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    Token const whileToken = tokens.peek();
    if (whileToken.kind() != While) {
        issues.push<UnqualifiedID>(whileToken, While);
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    Token const delimToken = tokens.peek();
    if (delimToken.kind() != Semicolon) {
        issues.push<UnqualifiedID>(whileToken, Semicolon);
        panic(tokens);
    }
    else {
        tokens.eat();
    }
    return allocate<ast::LoopStatement>(doToken.sourceRange(),
                                        ast::LoopKind::DoWhile,
                                        nullptr,
                                        std::move(cond),
                                        nullptr,
                                        std::move(block));
}

UniquePtr<ast::LoopStatement> Context::parseForStatement() {
    Token const forToken = tokens.peek();
    if (forToken.kind() != For) {
        return nullptr;
    }
    tokens.eat();
    auto varDecl = parseShortVariableDeclaration(forToken);
    if (!varDecl) {
        // TODO: This should be ExpectedDeclaration or something...
        pushExpectedExpression(tokens.peek());
        return nullptr;
    }
    auto cond = parseComma();
    if (!cond) {
        pushExpectedExpression(tokens.peek());
    }
    if (tokens.peek().kind() != Semicolon) {
        expectDelimiter(Semicolon);
        return nullptr;
    }
    tokens.eat();
    auto inc = parseComma();
    if (!inc) {
        pushExpectedExpression(tokens.peek());
    }
    auto block = parseCompoundStatement();
    if (!block) {
        issues.push<UnqualifiedID>(tokens.peek(), OpenBrace);
    }
    return allocate<ast::LoopStatement>(forToken.sourceRange(),
                                        ast::LoopKind::For,
                                        std::move(varDecl),
                                        std::move(cond),
                                        std::move(inc),
                                        std::move(block));
}

static std::optional<ast::JumpStatement::Kind> toJumpKind(Token token) {
    switch (token.kind()) {
    case Break:
        return ast::JumpStatement::Break;
    case Continue:
        return ast::JumpStatement::Continue;
    default:
        return std::nullopt;
    }
}

UniquePtr<ast::JumpStatement> Context::parseJumpStatement() {
    Token const token   = tokens.peek();
    auto const jumpKind = toJumpKind(token);
    if (!jumpKind) {
        return nullptr;
    }
    tokens.eat();
    expectDelimiter(Semicolon);
    return allocate<ast::JumpStatement>(*jumpKind, token.sourceRange());
}

UniquePtr<ast::EmptyStatement> Context::parseEmptyStatement() {
    Token const delim = tokens.peek();
    if (delim.kind() != Semicolon) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::EmptyStatement>(delim.sourceRange());
}

// MARK: - Expressions

UniquePtr<ast::Expression> Context::parseTypeExpression() {
    return parseReference();
}

UniquePtr<ast::Expression> Context::parseComma() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Comma>(
        [this] { return parseAssignment(); });
}

UniquePtr<ast::Expression> Context::parseAssignment() {
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
                                  XOrAssignment>(
        [this] { return parseConditional(); });
}

UniquePtr<ast::Expression> Context::parseConditional() {
    Token const& condToken = tokens.peek();
    auto logicalOr         = parseLogicalOr();
    if (auto const& questionMark = tokens.peek();
        questionMark.kind() == Question)
    {
        if (!logicalOr) {
            pushExpectedExpression(condToken);
        }
        tokens.eat();
        auto lhs = parseComma();
        if (!lhs) {
            pushExpectedExpression(tokens.peek());
        }
        Token const& colon = tokens.peek();
        if (colon.kind() != Colon) {
            issues.push<UnqualifiedID>(colon, Colon);
        }
        else {
            tokens.eat();
        }
        auto rhs = parseConditional();
        if (!rhs) {
            pushExpectedExpression(tokens.peek());
        }
        return allocate<ast::Conditional>(std::move(logicalOr),
                                          std::move(lhs),
                                          std::move(rhs),
                                          questionMark.sourceRange());
    }
    return logicalOr;
}

UniquePtr<ast::Expression> Context::parseLogicalOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalOr>(
        [this] { return parseLogicalAnd(); });
}

UniquePtr<ast::Expression> Context::parseLogicalAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalAnd>(
        [this] { return parseInclusiveOr(); });
}

UniquePtr<ast::Expression> Context::parseInclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseOr>(
        [this] { return parseExclusiveOr(); });
}

UniquePtr<ast::Expression> Context::parseExclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseXOr>(
        [this] { return parseAnd(); });
}

UniquePtr<ast::Expression> Context::parseAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseAnd>(
        [this] { return parseEquality(); });
}

UniquePtr<ast::Expression> Context::parseEquality() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Equals,
                                  ast::BinaryOperator::NotEquals>(
        [this] { return parseRelational(); });
}

UniquePtr<ast::Expression> Context::parseRelational() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Less,
                                  ast::BinaryOperator::LessEq,
                                  ast::BinaryOperator::Greater,
                                  ast::BinaryOperator::GreaterEq>(
        [this] { return parseShift(); });
}

UniquePtr<ast::Expression> Context::parseShift() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LeftShift,
                                  ast::BinaryOperator::RightShift>(
        [this] { return parseAdditive(); });
}

UniquePtr<ast::Expression> Context::parseAdditive() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Addition,
                                  ast::BinaryOperator::Subtraction>(
        [this] { return parseMultiplicative(); });
}

UniquePtr<ast::Expression> Context::parseMultiplicative() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Multiplication,
                                  ast::BinaryOperator::Division,
                                  ast::BinaryOperator::Remainder>(
        [this] { return parseUnary(); });
}

UniquePtr<ast::Expression> Context::parseUnary() {
    if (auto postfix = parseReference()) {
        return postfix;
    }
    Token const token = tokens.peek();
    auto makeResult   = [&](ast::UnaryPrefixOperator operatorType) {
        // FIXME: Why reference to token?
        Token const& unaryToken = tokens.peek();
        auto unary              = parseUnary();
        if (!unary) {
            pushExpectedExpression(unaryToken);
        }
        return allocate<ast::UnaryPrefixExpression>(operatorType,
                                                    std::move(unary),
                                                    token.sourceRange());
    };
    if (token.kind() == Plus) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Promotion);
    }
    else if (token.kind() == Minus) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Negation);
    }
    else if (token.kind() == Tilde) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::BitwiseNot);
    }
    else if (token.kind() == Exclam) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::LogicalNot);
    }
    else if (token.kind() == Increment) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Increment);
    }
    else if (token.kind() == Decrement) {
        tokens.eat();
        return makeResult(ast::UnaryPrefixOperator::Decrement);
    }
    else {
        return nullptr;
    }
}

UniquePtr<ast::Expression> Context::parseReference() {
    if (auto unique = parseUnique()) {
        return unique;
    }
    Token const refToken = tokens.peek();
    if (refToken.kind() != BitAnd) {
        return nullptr;
    }
    tokens.eat();
    bool const mut = eatMut();
    auto referred  = parseConditional();
    return allocate<ast::ReferenceExpression>(std::move(referred),
                                              refToken.sourceRange());
}

UniquePtr<ast::Expression> Context::parseUnique() {
    if (auto postFix = parsePostfix()) {
        return postFix;
    }
    Token const uniqueToken = tokens.peek();
    if (uniqueToken.kind() != Unique) {
        return nullptr;
    }
    tokens.eat();
    bool const mut = eatMut();
    auto initExpr  = parsePostfix();
    return allocate<ast::UniqueExpression>(std::move(initExpr),
                                           uniqueToken.sourceRange());
}

UniquePtr<ast::Expression> Context::parsePostfix() {
    auto primary = parsePrimary();
    if (!primary) {
        return nullptr;
    }
    while (true) {
        auto const& token = tokens.peek();
        if (token.kind() == OpenBracket) {
            primary = parseSubscript(std::move(primary));
        }
        else if (token.kind() == OpenParan) {
            primary = parseFunctionCall(std::move(primary));
        }
        else if (token.kind() == Dot) {
            primary = parseMemberAccess(std::move(primary));
        }
        else {
            break;
        }
    }
    return primary;
}

UniquePtr<ast::Expression> Context::parsePrimary() {
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
    if (auto result = parseThisLiteral()) {
        return result;
    }
    if (auto result = parseStringLiteral()) {
        return result;
    }
    auto const& token = tokens.peek();
    if (token.kind() == OpenParan) {
        tokens.eat();
        Token const commaToken           = tokens.peek();
        UniquePtr<ast::Expression> comma = parseComma();
        if (!comma) {
            pushExpectedExpression(commaToken);
        }
        expectDelimiter(CloseParan);
        return comma;
    }
    if (token.kind() == OpenBracket) {
        SourceRange sourceRange = token.sourceRange();
        tokens.eat();
        bool first = true;
        utl::small_vector<UniquePtr<ast::Expression>> elems;
        Token next = tokens.peek();
        while (next.kind() != CloseBracket) {
            if (!first) {
                expectDelimiter(Comma);
            }
            first     = false;
            auto expr = parseConditional();
            if (!expr) {
                pushExpectedExpression(next);
            }
            elems.push_back(std::move(expr));
            next = tokens.peek();
        }
        tokens.eat();
        return allocate<ast::ListExpression>(std::move(elems),
                                             merge(sourceRange,
                                                   next.sourceRange()));
    }
    return nullptr;
}

UniquePtr<ast::Identifier> Context::parseIdentifier() {
    Token const token = tokens.peek();
    if (!isIdentifier(token.kind())) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::Identifier>(token.sourceRange(), token.id());
}

UniquePtr<ast::IntegerLiteral> Context::parseIntegerLiteral() {
    if (tokens.peek().kind() != IntegerLiteral) {
        return nullptr;
    }
    auto token = tokens.eat();
    return allocate<ast::IntegerLiteral>(token.sourceRange(),
                                         token.toInteger(64));
}

UniquePtr<ast::BooleanLiteral> Context::parseBooleanLiteral() {
    Token const token = tokens.peek();
    if (token.kind() != True && token.kind() != False) {
        return nullptr;
    }
    tokens.eat();
    return allocate<ast::BooleanLiteral>(token.sourceRange(), token.toBool());
}

UniquePtr<ast::FloatingPointLiteral> Context::parseFloatingPointLiteral() {
    if (tokens.peek().kind() != FloatLiteral) {
        return nullptr;
    }
    auto token = tokens.eat();
    return allocate<ast::FloatingPointLiteral>(token.sourceRange(),
                                               token.toFloat(
                                                   APFloatPrec::Double));
}

UniquePtr<ast::ThisLiteral> Context::parseThisLiteral() {
    if (tokens.peek().kind() != This) {
        return nullptr;
    }
    auto token = tokens.eat();
    return allocate<ast::ThisLiteral>(token.sourceRange());
}

UniquePtr<ast::StringLiteral> Context::parseStringLiteral() {
    if (tokens.peek().kind() != StringLiteral) {
        return nullptr;
    }
    auto token = tokens.eat();
    return allocate<ast::StringLiteral>(token.sourceRange(), token.id());
}

template <typename FunctionCallLike>
UniquePtr<FunctionCallLike> Context::parseFunctionCallLike(
    UniquePtr<ast::Expression> primary, TokenKind open, TokenKind close) {
    auto const& openToken = tokens.peek();
    SC_ASSERT(openToken.kind() == open, "");
    auto args =
        parseList<utl::small_vector<UniquePtr<ast::Expression>>>(open,
                                                                 close,
                                                                 Comma,
                                                                 [this] {
        return parseAssignment();
        }).value(); // handle error
    return allocate<FunctionCallLike>(std::move(primary),
                                      std::move(args),
                                      openToken.sourceRange());
}

template <typename, typename List>
std::optional<List> Context::parseList(TokenKind open,
                                       TokenKind close,
                                       TokenKind delimiter,
                                       auto parseCallback) {
    auto const& openToken = tokens.peek();
    if (openToken.kind() != open) {
        return std::nullopt;
    }
    tokens.eat();
    List result;
    bool first = true;
    while (true) {
        Token const next = tokens.peek();
        if (next.kind() == close) {
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
            /// TODO: Maybe delegate error handling to the parse callback to
            /// avoid duplicate errors in some cases
            issues.push<ExpectedExpression>(tokens.peek());
            /// Without eating a token we may get stuck in an infinite loop,
            /// otherwise we may miss delimiters in case of syntax errors
            /// (especcially missing ')').
            //          tokens.eat();
        }
    }
    return result;
}

UniquePtr<ast::Subscript> Context::parseSubscript(
    UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::Subscript>(std::move(primary),
                                                 OpenBracket,
                                                 CloseBracket);
}

UniquePtr<ast::FunctionCall> Context::parseFunctionCall(
    UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::FunctionCall>(std::move(primary),
                                                    OpenParan,
                                                    CloseParan);
}

UniquePtr<ast::Expression> Context::parseMemberAccess(
    UniquePtr<ast::Expression> left) {
    SC_ASSERT(tokens.peek().kind() == Dot, "");
    while (true) {
        auto const& token = tokens.peek();
        if (token.kind() != Dot) {
            return left;
        }
        tokens.eat();
        auto right = parseIdentifier();
        if (!right) {
            issues.push<ExpectedExpression>(tokens.peek());
        }
        left = allocate<ast::MemberAccess>(std::move(left),
                                           std::move(right),
                                           token.sourceRange());
        continue;
    }
}

bool Context::eatMut() {
    if (tokens.peek().kind() == Mutable) {
        tokens.eat();
        return true;
    }
    return false;
}

void Context::pushExpectedExpression(Token const& token) {
    issues.push<ExpectedExpression>(token);
}

static TokenKind toTokenKind(ast::BinaryOperator op) {
    switch (op) {
    case ast::BinaryOperator::Multiplication:
        return Multiplies;
    case ast::BinaryOperator::Division:
        return Divides;
    case ast::BinaryOperator::Remainder:
        return Remainder;
    case ast::BinaryOperator::Addition:
        return Plus;
    case ast::BinaryOperator::Subtraction:
        return Minus;
    case ast::BinaryOperator::LeftShift:
        return LeftShift;
    case ast::BinaryOperator::RightShift:
        return RightShift;
    case ast::BinaryOperator::Less:
        return Less;
    case ast::BinaryOperator::LessEq:
        return LessEqual;
    case ast::BinaryOperator::Greater:
        return Greater;
    case ast::BinaryOperator::GreaterEq:
        return GreaterEqual;
    case ast::BinaryOperator::Equals:
        return Equal;
    case ast::BinaryOperator::NotEquals:
        return Unequal;
    case ast::BinaryOperator::BitwiseAnd:
        return BitAnd;
    case ast::BinaryOperator::BitwiseXOr:
        return BitXOr;
    case ast::BinaryOperator::BitwiseOr:
        return BitOr;
    case ast::BinaryOperator::LogicalAnd:
        return LogicalAnd;
    case ast::BinaryOperator::LogicalOr:
        return LogicalOr;
    case ast::BinaryOperator::Assignment:
        return Assign;
    case ast::BinaryOperator::AddAssignment:
        return PlusAssign;
    case ast::BinaryOperator::SubAssignment:
        return MinusAssign;
    case ast::BinaryOperator::MulAssignment:
        return MultipliesAssign;
    case ast::BinaryOperator::DivAssignment:
        return DividesAssign;
    case ast::BinaryOperator::RemAssignment:
        return RemainderAssign;
    case ast::BinaryOperator::LSAssignment:
        return LeftShiftAssign;
    case ast::BinaryOperator::RSAssignment:
        return RightShiftAssign;
    case ast::BinaryOperator::AndAssignment:
        return AndAssign;
    case ast::BinaryOperator::OrAssignment:
        return OrAssign;
    case ast::BinaryOperator::XOrAssignment:
        return XOrAssign;
    case ast::BinaryOperator::Comma:
        return Comma;
    default:
        SC_UNREACHABLE();
    }
}

template <ast::BinaryOperator... Op>
UniquePtr<ast::Expression> Context::parseBinaryOperatorLTR(auto&& operand) {
    Token const& lhsToken           = tokens.peek();
    UniquePtr<ast::Expression> left = operand();
    auto tryParse = [&](Token const token, ast::BinaryOperator op) {
        if (token.kind() != toTokenKind(op)) {
            return false;
        }
        tokens.eat();
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        Token const& rhsToken            = tokens.peek();
        UniquePtr<ast::Expression> right = operand();
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        left = allocate<ast::BinaryExpression>(op,
                                               std::move(left),
                                               std::move(right),
                                               token.sourceRange());
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
UniquePtr<ast::Expression> Context::parseBinaryOperatorRTL(
    auto&& parseOperand) {
    Token const& lhsToken = tokens.peek();
    SC_ASSERT(lhsToken.kind() != EndOfFile, "");
    UniquePtr<ast::Expression> left = parseOperand();
    Token const operatorToken       = tokens.peek();
    auto parse                      = [&](ast::BinaryOperator op) {
        if (!left) {
            pushExpectedExpression(lhsToken);
        }
        tokens.eat();
        Token const& rhsToken = tokens.peek();
        UniquePtr<ast::Expression> right =
            parseBinaryOperatorRTL<Op...>(parseOperand);
        if (!right) {
            pushExpectedExpression(rhsToken);
        }
        return allocate<ast::BinaryExpression>(op,
                                               std::move(left),
                                               std::move(right),
                                               operatorToken.sourceRange());
    };
    if (UniquePtr<ast::Expression> result = nullptr;
        ((operatorToken.kind() == toTokenKind(Op) &&
          ((result = parse(Op)), true)) ||
         ...))
    {
        return result;
    }
    return left;
}

void Context::expectDelimiter(TokenKind delimiter) {
    if (auto next = tokens.peek(); next.kind() != delimiter) {
        issues.push<ExpectedDelimiter>(next);
    }
    else {
        tokens.eat();
    }
}
