#include "Common/ProgramIssue.h"

#include <sstream>

namespace scatha {
	
	ProgramIssue::ProgramIssue(Token const& token, std::string_view brief, std::string_view message):
		std::runtime_error(makeWhatArg(token, brief, message)),
		_token(token)
	{}
	
	std::string ProgramIssue::makeWhatArg(Token const& token, std::string_view brief, std::string_view message) {
		std::stringstream sstr;
		sstr << brief << "\nLine: " << token.sourceLocation.line << ", Column: " << token.sourceLocation.column << "\n";
		if (!message.empty()) {
			sstr << message;
		}
		return sstr.str();
	}
	
}
