#include "ExpressionParser.h"

#include "Parser/ParserError.h"

namespace scatha::parse {

	
	Expression* ExpressionParser::parseExpression() {
		return parseE();
	}
	
	template <typename... T>
	Expression* ExpressionParser::parseET_impl(auto&& parseOperand, auto&&... id) {
		Expression* left = parseOperand();
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
				if (token.id != id) { return false; }
				tokens.eat();
				Expression* const right = parseOperand();
				left = allocate<T>(alloc, left, right);
				return true;
			}() || ...)) {
				continue;
			}
			
			return left;
		}
	}
	
	Expression* ExpressionParser::parseE() {
		return parseET_impl<Addition, Subtraction>([this]{ return parseT(); }, "+", "-");
	}
	
	Expression* ExpressionParser::parseT() {
		return parseET_impl<Multiplication, Division, Modulo>([this]{ return parseF(); }, "*", "/", "%");
	}
	
	Expression* ExpressionParser::parseF() {
		TokenEx const& token = tokens.peek();
		switch (token.type) {
			case TokenType::Identifier:
				tokens.eat();
				return allocate<Identifier>(alloc, token.id);
		
			case TokenType::NumericLiteral:
				tokens.eat();
				return allocate<NumericLiteral>(alloc, token.id);
				
			case TokenType::Punctuation: {
				if (token.id == "(") {
					tokens.eat();
					Expression* const e = parseE();
					
					TokenEx const& next = tokens.eat();
					if (next.id == ")") {
						return e;
					}
					else {
						throw ParserError(next, "Unqualified ID");
					}
				}
				break;
			}
			case TokenType::Operator: {
				if (token.id == "-") {
					tokens.eat();
					return allocate<Negation>(alloc, parseF());
				}
				break;
			}
				
			default:
				break;
		}
		throw ParserError(token, "Unqualified ID");
	}
	
}
