#ifndef SCATHA_PARSER_EXPRESSIONPARSER_H_
#define SCATHA_PARSER_EXPRESSIONPARSER_H_

#include "Common/Allocator.h"

#include "Parser/ParseTree.h"
#include "Parser/TokenStream.h"

namespace scatha::parse {
	
	/*
	 
	 /// MARK: Grammar
	 
	 Non-Terminals:
		E (Expression)
		T (Term, operand of addition and subtraction)
		F (Factor, operand of multiplication and division)
	 
	 E -> T{ +|- T }
	 
	 T -> F{ *|/ T }
	 
	 F -> Identifier
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
		
	private:
		Allocator& alloc;
		TokenStream& tokens;
	};

}

#endif // SCATHA_PARSER_EXPRESSIONPARSER_H_

