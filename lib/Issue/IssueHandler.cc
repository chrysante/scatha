#include "Issue/IssueHandler.h"

#include <utl/vector.hpp>

#include "Lexer/LexicalIssue.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace issue;

template <typename T>
struct issue::internal::IssueHandlerBase<T>::Impl {
    utl::vector<T> issues;
    bool fatal;
};

template <typename T>
issue::internal::IssueHandlerBase<T>::IssueHandlerBase(): impl(std::make_unique<Impl>()) {}

template <typename T>
issue::internal::IssueHandlerBase<T>::IssueHandlerBase(IssueHandlerBase&&) noexcept = default;

template <typename T>
issue::internal::IssueHandlerBase<T>::IssueHandlerBase::~IssueHandlerBase() = default;

template <typename T>
issue::internal::IssueHandlerBase<T>& issue::internal::IssueHandlerBase<T>::operator=(IssueHandlerBase&&) noexcept = default;

template <typename T>
void issue::internal::IssueHandlerBase<T>::push(T const& issue) {
    impl->issues.push_back(issue);
}

template <typename T>
void issue::internal::IssueHandlerBase<T>::push(T&& issue) {
    impl->issues.push_back(std::move(issue));
}

template <typename T>
void issue::internal::IssueHandlerBase<T>::push(T const& issue, Fatal) {
    push(issue);
    setFatal();
}

template <typename T>
void issue::internal::IssueHandlerBase<T>::push(T&& issue, Fatal) {
    push(std::move(issue));
    setFatal();
}

template <typename T>
std::span<T const> issue::internal::IssueHandlerBase<T>::issues() const {
    return impl->issues;
}

template <typename T>
bool issue::internal::IssueHandlerBase<T>::empty() const {
    return impl->issues.empty();
}

template <typename T>
bool issue::internal::IssueHandlerBase<T>::fatal() const {
    return impl->fatal;
}

template <typename T>
void issue::internal::IssueHandlerBase<T>::setFatal() {
    impl->fatal = true;
}

template class issue::internal::IssueHandlerBase<lex::LexicalIssue>;
template class issue::internal::IssueHandlerBase<parse::SyntaxIssue>;
template class issue::internal::IssueHandlerBase<sema::SemanticIssue>;
