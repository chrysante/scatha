#include "ProgramIssue.h"

#include <sstream>

namespace scatha {
	
	ProgramIssue::ProgramIssue(Token const& token, std::string_view message):
		std::runtime_error(makeWhatArg(token, message)),
		_token(token)
	{}
	
	std::string ProgramIssue::makeWhatArg(Token const& token, std::string_view message) {
		std::stringstream sstr;
		sstr << message << "\nLine: " << token.sourceLocation.line << ", Column: " << token.sourceLocation.column << "\n";
		return sstr.str();
	}
	
}
