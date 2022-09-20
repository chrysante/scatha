#pragma once

#ifndef SCATHA_LEXER_LEXERERROR_H_
#define SCATHA_LEXER_LEXERERROR_H_

#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"

namespace scatha::lex {
	
	class LexicalIssue: public ProgramIssue {
	protected:
		LexicalIssue(Token const&, std::string const& message);
	};
	
	class UnexpectedID: public LexicalIssue {
	public:
		explicit UnexpectedID(Token const&);
	};
	
	class InvalidNumericLiteral: public LexicalIssue {
	public:
		explicit InvalidNumericLiteral(Token const&);
	};
	
	class UnterminatedStringLiteral: public LexicalIssue {
	public:
		explicit UnterminatedStringLiteral(Token const&);
	};
	
}

#endif // SCATHA_LEXER_LEXERERROR_H_

