#include "ExpressionParser.h"

#include "Parser/ParserError.h"

namespace scatha::parse {

	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseExpression() {
		return parseComma();
	}
	
	template <ast::BinaryOperator... Op>
	ast::UniquePtr<ast::Expression> ExpressionParser::parseBinaryOperatorLTR(auto&& operand) {
		ast::UniquePtr<ast::Expression> left = operand();
		while (true) {
			/* Expands into:
			 if (token.id == id[0]) {
				...
			 }
			 else if (token.id == id[1]) {
				...
			 }
			 ...
			 else if (token.id == id[last]) {
				...
			 }
			 else {
				return left;
			 }
			 */
			TokenEx const& token = tokens.peek();
			if (([&]{
				if (token.id != toString(Op)) { return false; }
				tokens.eat();
				ast::UniquePtr<ast::Expression> right = operand();
				left = ast::allocate<ast::BinaryExpression>(Op, std::move(left), std::move(right), token);
				return true;
			}() || ...)) {
				continue;
			}
			
			return left;
		}
	}
	
	template <ast::BinaryOperator... Op>
	ast::UniquePtr<ast::Expression> ExpressionParser::parseBinaryOperatorRTL(auto&& parseOperand) {
		auto left = parseOperand();
		TokenEx const& token = tokens.peek();
		
		if (ast::UniquePtr<ast::Expression> result = nullptr; ((token.id == toString(Op) && (result = [&]{
			tokens.eat();
			ast::UniquePtr<ast::Expression> right = parseBinaryOperatorRTL<Op...>(parseOperand);
			return ast::allocate<ast::BinaryExpression>(Op, std::move(left), std::move(right), token);
		}(), true)) || ...)) {
			return result;
		}
		else {
			return left;
		}
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseComma() {
		return parseBinaryOperatorLTR<ast::BinaryOperator::Comma>([this]{ return parseAssignment(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseAssignment() {
		using enum ast::BinaryOperator;
		return parseBinaryOperatorRTL<
			Assignment, AddAssignment, SubAssignment, MulAssignment, DivAssignment, RemAssignment,
			LSAssignment, RSAssignment, AndAssignment, OrAssignment
		>([this]{ return parseConditional(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseConditional() {
		auto logicalOr = parseLogicalOr();
		
		if (auto const& token = tokens.peek();
			token.id == "?")
		{
			tokens.eat();
			auto lhs = parseComma();
			expectID(tokens.eat(), ":");
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
		return parseBinaryOperatorLTR<ast::BinaryOperator::Equals, ast::BinaryOperator::NotEquals>([this] { return parseRelational(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseRelational() {
		return parseBinaryOperatorLTR<
			ast::BinaryOperator::Less,    ast::BinaryOperator::LessEq,
			ast::BinaryOperator::Greater, ast::BinaryOperator::GreaterEq
		>([this] { return parseShift(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseShift() {
		return parseBinaryOperatorLTR<ast::BinaryOperator::LeftShift, ast::BinaryOperator::RightShift>([this] { return parseAdditive(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseAdditive() {
		return parseBinaryOperatorLTR<ast::BinaryOperator::Addition, ast::BinaryOperator::Subtraction>([this] { return parseMultiplicative(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseMultiplicative() {
		return parseBinaryOperatorLTR<ast::BinaryOperator::Multiplication, ast::BinaryOperator::Division, ast::BinaryOperator::Remainder>([this] { return parseUnary(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseUnary() {
		if (auto postfix = parsePostfix()) {
			return postfix;
		}
		TokenEx const& token = tokens.eat();
		
		if (token.id == "&") {
			assert(false && "Do we really want to support addressof operator?");
		}
		else if (token.id == "+") {
			return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::Promotion, parseUnary(), token);
		}
		else if (token.id == "-") {
			return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::Negation, parseUnary(), token);
		}
		else if (token.id == "~") {
			return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::BitwiseNot, parseUnary(), token);
		}
		else if (token.id == "!") {
			return ast::allocate<ast::UnaryPrefixExpression>(ast::UnaryPrefixOperator::LogicalNot, parseUnary(), token);
		}
		else {
			throw ParserError(token, "Unqualified ID");
		}
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parsePostfix() {
		auto primary = parsePrimary();
		if (!primary) { return nullptr; }
		
		while (true) {
			auto const& token = tokens.peek(false);
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
		TokenEx const& token = tokens.peek();
		switch (token.type) {
				// Identifier
			case TokenType::Identifier: {
				tokens.eat();
				return ast::allocate<ast::Identifier>(token);
			}
				// Numeric literal
			case TokenType::IntegerLiteral: {
				tokens.eat();
				return ast::allocate<ast::IntegerLiteral>(token);
			}
				// String literal
			case TokenType::StringLiteral: {
				tokens.eat();
				return ast::allocate<ast::StringLiteral>(token);
			}
				// Parenthesized comma expression
			case TokenType::Punctuation: {
				if (token.id == "(") {
					tokens.eat();
					ast::UniquePtr<ast::Expression> e = parseComma();
					
					TokenEx const& next = tokens.eat();
					expectID(next, ")");
					
					return e;
				}
				break;
			}
				
			default:
				break;
		}
		return nullptr;
	}
	
	template <typename FC>
	ast::UniquePtr<ast::Expression> ExpressionParser::parseFunctionCallLike(ast::UniquePtr<ast::Expression> primary,
																			std::string_view open, std::string_view close)
	{
		auto const& openToken = tokens.eat();
		assert(openToken.id == open);
		auto result = ast::allocate<FC>(std::move(primary), openToken);
		
		if (tokens.peek().id == close) { // no arguments
			tokens.eat();
			return result;
		}
		
		while (true) {
			result->arguments.push_back(parseAssignment());
			TokenEx const& next = tokens.eat();
			if (next.id == close) {
				break;
			}
			expectID(next, ",");
		}
		return result;
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseSubscript(ast::UniquePtr<ast::Expression> primary) {
		auto result = parseFunctionCallLike<ast::Subscript>(std::move(primary), "[", "]");
		// dynamic_cast is not really necessary but just to be safe...
		if (auto* ptr = dynamic_cast<ast::Subscript*>(result.get());
			ptr && ptr->arguments.empty())
		{
			throw ParserError(tokens.current(), "Subscript with no arguments is not allowed");
		}
		return result;
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseFunctionCall(ast::UniquePtr<ast::Expression> primary) {
		return parseFunctionCallLike<ast::FunctionCall>(std::move(primary), "(", ")");
	}

	ast::UniquePtr<ast::Expression> ExpressionParser::parseMemberAccess(ast::UniquePtr<ast::Expression> primary) {
		auto const& dot = tokens.eat();
		assert(dot.id == ".");
		auto const& id = tokens.eat();
		expectIdentifier(id);
		return ast::allocate<ast::MemberAccess>(std::move(primary), id, dot);
	}
	
}
