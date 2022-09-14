#include "ExpressionParser.h"

#include "Parser/ParserError.h"

namespace scatha::parse {

	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseExpression() {
		return parseComma();
	}
	
	template <typename... T>
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
				if (token.id != T::operatorString()) { return false; }
				tokens.eat();
				ast::UniquePtr<ast::Expression> right = operand();
				left = ast::allocate<T>(std::move(left), std::move(right));
				return true;
			}() || ...)) {
				continue;
			}
			
			return left;
		}
	}
	
	template <typename... T>
	ast::UniquePtr<ast::Expression> ExpressionParser::parseBinaryOperatorRTL(auto&& parseOperand) {
		auto left = parseOperand();
		TokenEx const& nextToken = tokens.peek();
		
		if (ast::UniquePtr<ast::Expression> result = nullptr; ((nextToken.id == T::operatorString() && (result = [&]{
			tokens.eat();
			ast::UniquePtr<ast::Expression> right = parseBinaryOperatorRTL<T...>(parseOperand);
			return ast::allocate<T>(std::move(left), std::move(right));
		}(), true)) || ...)) {
			return result;
		}
		else {
			return left;
		}
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseComma() {
		return parseBinaryOperatorLTR<ast::Comma>([this]{ return parseAssignment(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseAssignment() {
		return parseBinaryOperatorRTL<
			ast::Assignment, ast::AddAssignment, ast::SubAssignment, ast::MulAssignment, ast::DivAssignment, ast::RemAssignment,
			ast::LSAssignment, ast::RSAssignment, ast::AndAssignment, ast::OrAssignment
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
			return allocate<ast::Conditional>(std::move(logicalOr), std::move(lhs), std::move(rhs));
		}
		
		return logicalOr;
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseLogicalOr() {
		return parseBinaryOperatorLTR<ast::LogicalOr>([this] { return parseLogicalAnd(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseLogicalAnd() {
		return parseBinaryOperatorLTR<ast::LogicalAnd>([this] { return parseInclusiveOr(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseInclusiveOr() {
		return parseBinaryOperatorLTR<ast::BitwiseOr>([this] { return parseExclusiveOr(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseExclusiveOr() {
		return parseBinaryOperatorLTR<ast::BitwiseXOr>([this] { return parseAnd(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseAnd() {
		return parseBinaryOperatorLTR<ast::BitwiseAnd>([this] { return parseEquality(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseEquality() {
		return parseBinaryOperatorLTR<ast::Equals, ast::NotEquals>([this] { return parseRelational(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseRelational() {
		return parseBinaryOperatorLTR<ast::Less, ast::LessEq, ast::Greater, ast::GreaterEq>([this] { return parseShift(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseShift() {
		return parseBinaryOperatorLTR<ast::LeftShift, ast::RightShift>([this] { return parseAdditive(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseAdditive() {
		return parseBinaryOperatorLTR<ast::Addition, ast::Subtraction>([this] { return parseMultiplicative(); });
	}
	
	ast::UniquePtr<ast::Expression> ExpressionParser::parseMultiplicative() {
		return parseBinaryOperatorLTR<ast::Multiplication, ast::Division, ast::Remainder>([this] { return parseUnary(); });
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
			return ast::allocate<ast::Promotion>(parseUnary());
		}
		else if (token.id == "-") {
			return ast::allocate<ast::Negation>(parseUnary());
		}
		else if (token.id == "~") {
			return ast::allocate<ast::BitwiseNot>(parseUnary());
		}
		else if (token.id == "!") {
			return ast::allocate<ast::LogicalNot>(parseUnary());
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
				return ast::allocate<ast::Identifier>(token.id);
			}
				// Numeric literal
			case TokenType::NumericLiteral: {
				tokens.eat();
				return ast::allocate<ast::NumericLiteral>(token.id);
			}
				// String literal
			case TokenType::StringLiteral: {
				tokens.eat();
				return ast::allocate<ast::StringLiteral>(token.id);
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
		auto result = ast::allocate<FC>(std::move(primary));
		
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
		return ast::allocate<ast::MemberAccess>(std::move(primary), id.id);
	}
	
}
