#include "Issue/IssueHandler.h"

#include <utl/vector.hpp>

#include "Sema/SemanticIssue.h"

namespace scatha::issue {

	struct IssueHandler::Impl {
		utl::vector<sema::SemanticIssue> semaIssues;
	};
	
	IssueHandler::IssueHandler():
		impl(std::make_unique<Impl>())
	{
		
	}

	IssueHandler::IssueHandler(IssueHandler&&) noexcept = default;
	
	IssueHandler::~IssueHandler() = default;

	IssueHandler& IssueHandler::operator=(IssueHandler&&) noexcept = default;
	
	void IssueHandler::push(sema::SemanticIssue issue) {
		impl->semaIssues.push_back(std::move(issue));
	}
	
	std::span<sema::SemanticIssue const> IssueHandler::semaIssues() const {
		return impl->semaIssues;
	}
	
	bool IssueHandler::empty() const {
		return impl->semaIssues.empty();
	}
	
}
