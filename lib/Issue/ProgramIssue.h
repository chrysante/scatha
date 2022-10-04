#ifndef SCATHA_ISSUE_PROGRAMISSUE_H_
#define SCATHA_ISSUE_PROGRAMISSUE_H_

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::issue::internal {
	/// So we can accept this as a parameter for default case in visitors.
	class SCATHA(API) ProgramIssuePrivateBase{};
}

namespace scatha::issue {
	
	class SCATHA(API) ProgramIssueBase: public internal::ProgramIssuePrivateBase {
	public:
		explicit ProgramIssueBase(Token token): _tok(std::move(token)) {}
		
		Token const& token() const { return _tok; }
		
		void setToken(Token token) { _tok = std::move(token); }
		
	private:
		Token _tok;
	};
	
}

#endif // SCATHA_ISSUE_PROGRAMISSUE_H_

