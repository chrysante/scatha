#ifndef SCATHA_PARSER_EXPRESSIONPARSER_H_
#define SCATHA_PARSER_EXPRESSIONPARSER_H_

#include "AST/Expression.h"
#include "Common/Allocator.h"
#include "Parser/TokenStream.h"

namespace scatha::parse {

	/*
	 
	 Precedence	| Operator			| Description					| Associativity
	 -----------+-------------------+-------------------------------+-------------------
		 1		| () 				| Function call					| Left to right ->
				| [] 				| Subscript						|
				| . 				| Member access					|
	 -----------+-------------------+-------------------------------+-------------------
		 2		| +, -				| Unary plus and minus			| Right to left <-
				| !, ~				| Logical and bitwise NOT		|
			    | &					| 								|
	 -----------+-------------------+-------------------------------+-------------------
		 3		| *, /, %			| Multiplication, division and	| Left to right ->
				|					| remainder						|
		 4		| +, -				| Addition and subtraction		|
		 5		| <<, >>			| Bitwise left and right shift	|
		 6		| <, <=, >, >=		| Relational operators			|
		 7		| ==, !=			| Equality operators			|
		 8		| &					| Bitwise AND					|
		 9		| ^					| Bitwise XOR					|
		10		| |					| Bitwise OR					|
		11		| &&				| Logical AND					|
		12		| ||				| Logical OR					|
	 -----------+-------------------+-------------------------------+-------------------
		13		| ?:				| Conditional					| Right to left <-
				| =, +=, -=			| Assignment					| Right to left <-
				| *=, /=, %=		| 								|
				| <<=, >>=, 		| 								|
				| &=, |=, 			| 								|
	 -----------+-------------------+-------------------------------+-------------------
		14		| ,					| Comma operator				| Left to right ->
	 
	
	*/
	
	/*
	 
	 /// MARK: Grammar
	
	 <comma-expression>				-->> <assignment-expression>
									   | <comma-expression> "," <assignment-expression>
	 <assignment-expression>		-->> <conditional-expression>
							    	   | <conditional-expression> "=, *=, ..." <assignment-expression>
	 <conditional-expression>		-->> <logical-or-expression>
									   | <logical-or-expression> "?" <comma-expression> ":" <conditional-expression>
	 <logical-or-expression>		-->> <logical-and-expression>
									   | <logical-or-expression> "||" <logical-and-expression>
	 <logical-and-expression>		-->> <inclusive-or-expression>
									   | <logical-and-expression> "&&" <inclusive-or-expression>
	 <inclusive-or-expression>		-->> <exclusive-or-expression>
							    	   | <inclusive-or-expression> "|" <exclusive-or-expression>
	 <exclusive-or-expression>		-->> <and-expression>
									   | <exclusive-or-expression> "^" <and-expression>
	 <and-expression> 		    	-->> <equality-expression>
									   | <and-expression> "&" <equality-expression>
	 <equality-expression> 			-->> <relational-expression>
									   | <equality-expression> "==" <relational-expression>
									   | <equality-expression> "!=" <relational-expression>
	 <relational-expression>		-->> <shift-expression>
									   | <relational-expression> "<"  <shift-expression>
									   | <relational-expression> ">"  <shift-expression>
									   | <relational-expression> "<=" <shift-expression>
									   | <relational-expression> ">=" <shift-expression>
	 <shift-expression>				-->> <additive-expression>
									   | <shift-expression> "<<" <additive-expression>
									   | <shift-expression> ">>" <additive-expression>
	 <additive-expression>			-->> <multiplicative-expression>
									   | <additive-expression> "+" <multiplicative-expression>
									   | <additive-expression> "-" <multiplicative-expression>

	 <multiplicative-expression>	-->> <unary-expression>
									   | <multiplicative-expression> "*" <unary-expression>
									   | <multiplicative-expression> "/" <unary-expression>
									   | <multiplicative-expression> "%" <unary-expression>
	 <unary-expression>				-->> <postfix-expression>
									   | "&" <unary-expression>
									   | "+" <unary-expression>
									   | "-" <unary-expression>
									   | "~" <unary-expression>
									   | "!" <unary-expression>
	 <postfix-expression>			-->> <primary-expression>
									   | <postfix-expression> "[" {<assignment-expression>}+ "]"
									   | <postfix-expression> "(" {<assignment-expression>}* ")"
									   | <postfix-expression> "." <identifier>
	 <primary-expression> 			-->> <identifier>
									   | <numeric-literal>
									   | <string-literal>
									   | "(" <comma-expression> ")"
	 
	 
	 
	 */
	
	class ExpressionParser {
	public:
		explicit ExpressionParser(TokenStream& tokens): tokens(tokens) {}
		
		ast::UniquePtr<ast::Expression> parseExpression();
		
	private:
		ast::UniquePtr<ast::Expression> parseComma();
		ast::UniquePtr<ast::Expression> parseAssignment();
		ast::UniquePtr<ast::Expression> parseConditional();
		ast::UniquePtr<ast::Expression> parseLogicalOr();
		ast::UniquePtr<ast::Expression> parseLogicalAnd();
		ast::UniquePtr<ast::Expression> parseInclusiveOr();
		ast::UniquePtr<ast::Expression> parseExclusiveOr();
		ast::UniquePtr<ast::Expression> parseAnd();
		ast::UniquePtr<ast::Expression> parseEquality();
		ast::UniquePtr<ast::Expression> parseRelational();
		ast::UniquePtr<ast::Expression> parseShift();
		ast::UniquePtr<ast::Expression> parseAdditive();
		ast::UniquePtr<ast::Expression> parseMultiplicative();
		ast::UniquePtr<ast::Expression> parseUnary();
		ast::UniquePtr<ast::Expression> parsePostfix();
		ast::UniquePtr<ast::Expression> parsePrimary();
		
		
		template <typename...>
		ast::UniquePtr<ast::Expression> parseBinaryOperatorLTR(auto&& operand);

		template <typename...>
		ast::UniquePtr<ast::Expression> parseBinaryOperatorRTL(auto&& parseOperand);
		
	private:
		TokenStream& tokens;
	};

}

#endif // SCATHA_PARSER_EXPRESSIONPARSER_H_

