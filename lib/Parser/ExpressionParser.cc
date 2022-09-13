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
			case TokenType::Identifier: {
				tokens.eat();
				auto* const identifier = allocate<Identifier>(alloc, token.id);
				if (tokens.peek().id == "(") { // We have an ID function call expression.
					tokens.eat();
					FunctionCall* result = allocate<FunctionCall>(alloc);
					result->object = identifier;
					
					if (tokens.peek().id == ")") { // function with no arguments
						tokens.eat();
						return result;
					}
					
					utl::small_vector<Expression*> arguments;
					while (true) {
						arguments.push_back(parseE());
						TokenEx const& next = tokens.eat();
						if (next.id == ")") {
							break;
						}
						if (next.id != ",") {
							throw ParserError(next, "Unqualified ID");
						}
					}
					result->arguments = allocateArray<Expression*>(alloc, arguments.begin(), arguments.end());
					return result;
				}
				else {
					return identifier;
				}
			}
		
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
				if (token.id == "+") {
					tokens.eat();
					return allocate<Promotion>(alloc, parseF());
				}
				else if (token.id == "-") {
					tokens.eat();
					return allocate<Negation>(alloc, parseF());
				}
				else {
					throw ParserError(token, "Unqualified ID");
				}
			}
				
			default:
				break;
		}
		throw ParserError(token, "Unqualified ID");
	}
	
}
