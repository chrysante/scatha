#ifndef SCATHA_ISSUE_ISSUEHANDLER_H_
#define SCATHA_ISSUE_ISSUEHANDLER_H_

#include <memory>
#include <span>

#include "Basic/Basic.h"

namespace scatha::sema { class SemanticIssue; }

namespace scatha::issue {
	
	class SCATHA(API) IssueHandler {
	public:
		IssueHandler();
		IssueHandler(IssueHandler&&) noexcept;
		~IssueHandler();
		
		IssueHandler& operator=(IssueHandler&&) noexcept;
		
		void push(sema::SemanticIssue);
		
		std::span<sema::SemanticIssue const> semaIssues() const;
		
		bool fatal() const { return _fatal; }
		void setFatal() { _fatal = true; }
		
		bool empty() const;
		
	private:
		struct Impl;
		
	private:
		std::unique_ptr<Impl> impl;
		bool _fatal = false;
	};
	
}

#endif // SCATHA_ISSUE_ISSUEHANDLER_H_

