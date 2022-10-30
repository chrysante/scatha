#include "Parser/ExpressionParser.h"

#include "Parser/ParsingIssue.h"

namespace scatha::parse {

using enum ParsingIssue::Reason;

ast::UniquePtr<ast::Expression> ExpressionParser::parseExpression() {
    return parseComma();
}

template <ast::BinaryOperator... Op>
ast::UniquePtr<ast::Expression> ExpressionParser::parseBinaryOperatorLTR(auto&& operand) {
    ast::UniquePtr<ast::Expression> left = operand();
//    if (!left) {
//        return nullptr;
//    }
    // -
    auto tryParse = [&](Token const& token, ast::BinaryOperator op) {
        if (token.id != toString(op)) {
            return false;
        }
        tokens.eat();
        ast::UniquePtr<ast::Expression> right = operand();
        left = ast::allocate<ast::BinaryExpression>(op, std::move(left), std::move(right), token);
        return true;
    };
    while (true) {
        Token const& token = tokens.peek();
        if ((tryParse(token, Op) || ...)) {
            continue;
        }

        return left;
    }
}

template <ast::BinaryOperator... Op>
ast::UniquePtr<ast::Expression> ExpressionParser::parseBinaryOperatorRTL(auto&& parseOperand) {
    auto left          = parseOperand();
    Token const& token = tokens.peek();
    auto parse         = [&](ast::BinaryOperator op) {
        tokens.eat();
        ast::UniquePtr<ast::Expression> right = parseBinaryOperatorRTL<Op...>(parseOperand);
        return ast::allocate<ast::BinaryExpression>(op, std::move(left), std::move(right), token);
    };
    if (ast::UniquePtr<ast::Expression> result = nullptr; ((token.id == toString(Op) && (result = parse(Op))) || ...)) {
        return result;
    }
    return left;
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseComma() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Comma>([this] { return parseAssignment(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseAssignment() {
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

ast::UniquePtr<ast::Expression> ExpressionParser::parseConditional() {
    auto logicalOr = parseLogicalOr();
    if (auto const& token = tokens.peek(); token.id == "?") {
        tokens.eat();
        auto lhs = parseComma();
        expectID(iss, tokens.eat(), ":");
        auto rhs = parseConditional();
        return allocate<ast::Conditional>(std::move(logicalOr), std::move(lhs), std::move(rhs), token);
    }
    return logicalOr;
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseLogicalOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalOr>([this] { return parseLogicalAnd(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseLogicalAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LogicalAnd>([this] { return parseInclusiveOr(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseInclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseOr>([this] { return parseExclusiveOr(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseExclusiveOr() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseXOr>([this] { return parseAnd(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseAnd() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::BitwiseAnd>([this] { return parseEquality(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseEquality() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Equals, ast::BinaryOperator::NotEquals>(
        [this] { return parseRelational(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseRelational() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Less,
                                  ast::BinaryOperator::LessEq,
                                  ast::BinaryOperator::Greater,
                                  ast::BinaryOperator::GreaterEq>([this] { return parseShift(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseShift() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::LeftShift, ast::BinaryOperator::RightShift>(
        [this] { return parseAdditive(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseAdditive() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Addition, ast::BinaryOperator::Subtraction>(
        [this] { return parseMultiplicative(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseMultiplicative() {
    return parseBinaryOperatorLTR<ast::BinaryOperator::Multiplication,
                                  ast::BinaryOperator::Division,
                                  ast::BinaryOperator::Remainder>([this] { return parseUnary(); });
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseUnary() {
    if (auto postfix = parsePostfix()) {
        return postfix;
    }
    Token const& token = tokens.peek();
    if (token.id == "&") {
        SC_DEBUGFAIL(); // Do we really want to support addressof operator?
    }
    else if (token.id == "+") {
        tokens.eat();
        return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::Promotion, parseUnary(), token);
    }
    else if (token.id == "-") {
        tokens.eat();
        return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::Negation, parseUnary(), token);
    }
    else if (token.id == "~") {
        tokens.eat();
        return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::BitwiseNot, parseUnary(), token);
    }
    else if (token.id == "!") {
        tokens.eat();
        return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::LogicalNot, parseUnary(), token);
    }
    else {
        iss.push(ParsingIssue(token, ExpectedExpression));
        return nullptr;
    }
}

ast::UniquePtr<ast::Expression> ExpressionParser::parsePostfix() {
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

ast::UniquePtr<ast::Expression> ExpressionParser::parsePrimary() {
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
        ast::UniquePtr<ast::Expression> e = parseComma();
        Token const& next                 = tokens.eat();
        expectID(iss, next, ")");
        return e;
    }
    return nullptr;
}

ast::UniquePtr<ast::Identifier> ExpressionParser::parseIdentifier() {
    Token const& next = tokens.peek();
    if (next.type != TokenType::Identifier || next.isDeclarator) {
        return nullptr;
    }
    return ast::allocate<ast::Identifier>(tokens.eat());
}

ast::UniquePtr<ast::IntegerLiteral> ExpressionParser::parseIntegerLiteral() {
    if (tokens.peek().type != TokenType::IntegerLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::IntegerLiteral>(tokens.eat());
}

ast::UniquePtr<ast::BooleanLiteral> ExpressionParser::parseBooleanLiteral() {
    if (tokens.peek().type != TokenType::BooleanLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::BooleanLiteral>(tokens.eat());
}

ast::UniquePtr<ast::FloatingPointLiteral> ExpressionParser::parseFloatingPointLiteral() {
    if (tokens.peek().type != TokenType::FloatingPointLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::FloatingPointLiteral>(tokens.eat());
}

ast::UniquePtr<ast::StringLiteral> ExpressionParser::parseStringLiteral() {
    if (tokens.peek().type != TokenType::StringLiteral) {
        return nullptr;
    }
    return ast::allocate<ast::StringLiteral>(tokens.eat());
}

template <typename FunctionCallLike>
ast::UniquePtr<FunctionCallLike> ExpressionParser::parseFunctionCallLike(ast::UniquePtr<ast::Expression> primary,
                                                                         std::string_view open,
                                                                         std::string_view close) {
    auto const& openToken = tokens.eat();
    SC_ASSERT(openToken.id == open, "");
    auto result = ast::allocate<FunctionCallLike>(std::move(primary), openToken);
    if (tokens.peek().id == close) { // no arguments
        tokens.eat();
        return result;
    }
    while (true) {
        result->arguments.push_back(parseAssignment());
        Token const& next = tokens.eat();
        if (next.id == close) {
            break;
        }
        expectID(iss, next, ",");
    }
    return result;
}

ast::UniquePtr<ast::Subscript> ExpressionParser::parseSubscript(ast::UniquePtr<ast::Expression> primary) {
    auto result = parseFunctionCallLike<ast::Subscript>(std::move(primary), "[", "]");
    // This is a semantic error 
//    if (result->arguments.empty()) {
//        throw ParsingIssue(tokens.current(), "Subscript with no arguments is not allowed");
//    }
    return result;
}

ast::UniquePtr<ast::FunctionCall> ExpressionParser::parseFunctionCall(ast::UniquePtr<ast::Expression> primary) {
    return parseFunctionCallLike<ast::FunctionCall>(std::move(primary), "(", ")");
}

ast::UniquePtr<ast::Expression> ExpressionParser::parseMemberAccess(ast::UniquePtr<ast::Expression> left) {
    SC_ASSERT(tokens.peek().id == ".", "");
    while (true) {
        auto const& token = tokens.peek();
        if (token.id != ".") {
            return left;
        }
        tokens.eat();
        ast::UniquePtr<ast::Expression> right = parseIdentifier();
        if (!right) {
            iss.push(ParsingIssue(tokens.peek(), ExpectedExpression));
        }
        left = ast::allocate<ast::MemberAccess>(std::move(left), std::move(right), token);
        continue;
    }
}

} // namespace scatha::parse
