#ifndef SCATHA_PARSER_PARSERERROR_H_
#define SCATHA_PARSER_PARSERERROR_H_

#include <stdexcept>
#include <string>

#include "Basic/Basic.h"
#include "Parser/TokenEx.h"

namespace scatha::parse {
	
	struct ParserError: std::runtime_error {
		explicit ParserError(TokenEx const&, std::string const& what);
		const char* what() const noexcept override;
		
		std::string _what;
		TokenEx token;
	};
	
}
	
#endif // SCATHA_PARSER_PARSERERROR_H_

