#ifndef SCATHA_PARSER_EXPRESSIONPARSER_H_
#define SCATHA_PARSER_EXPRESSIONPARSER_H_

#include "Common/Allocator.h"

#include "Parser/ParseTree.h"
#include "Parser/TokenStream.h"

namespace scatha::parse {

	/*
	 
	 Operator		| Description					| Associativity
	 ---------------+-------------------------------+-------------------
	 () 			| Function call					| Left to right ->
	 [] 			| Subscript						|
	 . 				| Member access					|
	 ---------------+-------------------------------+-------------------
	 +, -			| Unary plus and minus			| Right to left <-
	 !, ~			| Logical and bitwise NOT		|
	 ---------------+-------------------------------+-------------------
	 *, /, %		| Multiplication, division and	| Left to right ->
					| remainder						|
	 +, -			| Addition and subtraction		|
	 <<, >>			| Bitwise left and right shift	|
	 <, <=, >, >=	| Relational operators			|
	 ==, !=			| Equality operators			|
	 &				| Bitwise AND					|
	 ^				| Bitwise XOR					|
	 |				| Bitwise OR					|
	 &&				| Logical AND					|
	 ||				| Logical OR					|
	 ---------------+-------------------------------+-------------------
	 =				| Assignment					| Right to left <-
	 +=, -=			| 								|
	 *=, /=, %=		| 								|
	 <<=, >>=, 		| 								|
	 &=, |=, 		| 								|
	 
	
	*/
	
	/*
	 
	 /// MARK: Grammar
	 
	 Non-Terminals:
		E (Expression)
		T (Term, operand of addition and subtraction)
		F (Factor, operand of multiplication and division)
	 
	 1.
	 E -> T{ +|- T }
	 
	 2.
	 T -> F{ *|/|% F }
	 
	 3.
	 F -> ID(E...)           // Function call
	 F -> ID
	 F -> NumericLiteral
	 F -> (E) 
	 F -> +F
	 F -> -F
	 
	 */
	
	using Allocator = MonotonicBufferAllocator;
	
	class ExpressionParser {
	public:
		explicit ExpressionParser(TokenStream& tokens, Allocator& alloc): tokens(tokens), alloc(alloc) {}
		
		Expression* parseExpression();
		
	private:
		/// Parse Factor
		Expression* parseE();
		Expression* parseT();
		Expression* parseF();
		
		template <typename...>
		Expression* parseET_impl(auto&& parseOperand, auto&&...);
		
	private:
		Allocator& alloc;
		TokenStream& tokens;
	};

}

#endif // SCATHA_PARSER_EXPRESSIONPARSER_H_

