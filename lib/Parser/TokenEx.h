#pragma once

#ifndef SCATHA_PARSER_TOKENEX_H_
#define SCATHA_PARSER_TOKENEX_H_

#include <span>

#include "Common/Token.h"
#include "Parser/Keyword.h"

namespace scatha {
	
	enum class IdentifierCategory {
		Type, Variable, Function
	};
	
	struct TokenEx: Token {
		
		bool isSeparator   : 1 = false;
		bool isEOL         : 1 = false;
		bool isIdentifier  : 1 = false;
		bool isKeyword     : 1 = false;
		bool isPunctuation : 1 = false;
		
		// Keyword related fields
		parse::Keyword keyword{};
		parse::KeywordCategory keywordCategory{};
		
		// Identifier related fields
		IdentifierCategory identifierCategory{};
		
	};

	TokenEx expand(Token const&);
	
}

#endif // SCATHA_PARSER_TOKENEX_H_


