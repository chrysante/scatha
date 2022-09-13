#include "ExpressionParser.h"

#include "Parser/ParserError.h"

namespace scatha::parse {

	
	Expression* ExpressionParser::parseExpression() {
		return parseE();
	}
	
	Expression* ExpressionParser::parseE() {
		Expression* left = parseT();
		while (true) {
			TokenEx const& token = tokens.peek();
			if (token.id == "+") {
				tokens.eat();
				Expression* const right = parseT();
				left = allocate<Addition>(alloc, left, right);
			}
			else if (token.id == "-") {
				tokens.eat();
				Expression* const right = parseT();
				left = allocate<Subtraction>(alloc, left, right);
			}
			else {
				return left;
			}
		}
	}
	
	Expression* ExpressionParser::parseT() {
		Expression* left = parseF();
		while (true) {
			TokenEx const& token = tokens.peek();
			if (token.id == "*") {
				tokens.eat();
				Expression* const right = parseT();
				left = allocate<Multiplication>(alloc, left, right);
			}
			else if (token.id == "/") {
				tokens.eat();
				Expression* const right = parseT();
				left = allocate<Division>(alloc, left, right);
			}
			else {
				return left;
			}
		}
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
					
					TokenEx const& next = tokens.peek();
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
