#pragma once

#ifndef SCATHA_LEXER_LEXER_H_
#define SCATHA_LEXER_LEXER_H_

#include <string>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Common/SourceLocation.h"

namespace scatha::lex {
	
	class Lexer {
	public:
		explicit Lexer(std::string_view text);
		
		[[nodiscard]] utl::vector<Token> lex();
		
	private:
		void beginToken(TokenType type);
		void submitCurrentToken();
		
		bool lexOperator(char);
		bool lexStringLiteral(char c);
		
	private:
		std::string_view text;
		
		utl::vector<Token> result;
		
		SourceLocation sc;
		
		bool lexingToken = false;
		Token currentToken;
	};
	
	
	
}

#endif // SCATHA_LEXER_LEXER_H_
