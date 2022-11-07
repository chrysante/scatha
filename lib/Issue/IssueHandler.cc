#include "Issue/IssueHandler.h"

#include <utl/vector.hpp>

#include "Lexer/LexicalIssue.h"
#include "Parser/SyntaxIssue.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace issue;

template <typename T>
struct internal::IssueHandlerBase<T>::Impl {
    utl::vector<T> issues;
    bool fatal;
};

template <typename T>
internal::IssueHandlerBase<T>::IssueHandlerBase(): impl(std::make_unique<Impl>()) {}

template <typename T>
internal::IssueHandlerBase<T>::IssueHandlerBase(IssueHandlerBase&&) noexcept = default;

template <typename T>
internal::IssueHandlerBase<T>::~IssueHandlerBase() = default;

template <typename T>
internal::IssueHandlerBase<T>& internal::IssueHandlerBase<T>::operator=(IssueHandlerBase&&) noexcept = default;

template <typename T>
void internal::IssueHandlerBase<T>::push(T const& issue) {
    impl->issues.push_back(issue);
}

template <typename T>
void internal::IssueHandlerBase<T>::push(T&& issue) {
    impl->issues.push_back(std::move(issue));
}

template <typename T>
void internal::IssueHandlerBase<T>::push(T const& issue, Fatal) {
    push(issue);
    setFatal();
}

template <typename T>
void internal::IssueHandlerBase<T>::push(T&& issue, Fatal) {
    push(std::move(issue));
    setFatal();
}

template <typename T>
std::span<T const> internal::IssueHandlerBase<T>::issues() const {
    return impl->issues;
}

template <typename T>
bool internal::IssueHandlerBase<T>::empty() const { return impl->issues.empty(); }

template <typename T>
bool internal::IssueHandlerBase<T>::fatal() const { return impl->fatal; }

template <typename T>
void internal::IssueHandlerBase<T>::setFatal() { impl->fatal = true; }

template class internal::IssueHandlerBase<lex::LexicalIssue>;
template class internal::IssueHandlerBase<parse::SyntaxIssue>;
template class internal::IssueHandlerBase<sema::SemanticIssue>;
