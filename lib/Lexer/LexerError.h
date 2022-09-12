#pragma once

#ifndef SCATHA_LEXER_LEXERERROR_H_
#define SCATHA_LEXER_LEXERERROR_H_

#include <stdexcept>
#include <string>

#include "Basic/Basic.h"
#include "Common/SourceLocation.h"

namespace scatha::lex {
	
	struct LexerError: std::runtime_error {
		explicit LexerError(SourceLocation sc, std::string const& what);
		const char* what() const noexcept override;
		
		std::string _what;
		SourceLocation sourceLocation;
	};
	
}

namespace scatha::internal {
	
	std::string makeWhatArg(std::string const&, SourceLocation const&);
	
}

#endif // SCATHA_LEXER_LEXERERROR_H_

