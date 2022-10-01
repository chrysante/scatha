#ifndef SCATHA_ISSUE_PROGRAMISSUE_H_
#define SCATHA_ISSUE_PROGRAMISSUE_H_

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::issue {
	
	class SCATHA(API) ProgramIssue {
	public:
		explicit ProgramIssue(Token token): _tok(std::move(token)) {}
		
		Token const& token() const { return _tok; }
		
	private:
		Token _tok;
	};
	
}

#endif // SCATHA_ISSUE_PROGRAMISSUE_H_

