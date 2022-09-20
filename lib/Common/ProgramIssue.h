#ifndef SCATHA_COMMON_ISSUE_H_
#define SCATHA_COMMON_ISSUE_H_

#include <stdexcept>
#include <string>

#include "Common/Token.h"

namespace scatha {
	
	class ProgramIssue: public std::runtime_error {
	protected:
		explicit ProgramIssue(Token const&, std::string_view message);
		
		Token const& token() const { return _token; }
		SourceLocation const& sourceLocation() const { return token().sourceLocation; }
		
	private:
		static std::string makeWhatArg(Token const&, std::string_view message);
		
	private:
		Token _token;
	};
	
}

#endif // SCATHA_COMMON_ISSUE_H_

