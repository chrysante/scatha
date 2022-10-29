#include "Issue/IssueHandler.h"

#include <utl/vector.hpp>

#include "Lexer/LexicalIssue.h"
#include "Sema/SemanticIssue.h"

namespace scatha::issue {

struct IssueHandler::Impl {
    utl::vector<lex::LexicalIssue> lexicalIssues;
    utl::vector<sema::SemanticIssue> semaIssues;
};

IssueHandler::IssueHandler(): impl(std::make_unique<Impl>()) {}

IssueHandler::IssueHandler(IssueHandler&&) noexcept = default;

IssueHandler::~IssueHandler() = default;

IssueHandler& IssueHandler::operator=(IssueHandler&&) noexcept = default;

void IssueHandler::push(lex::LexicalIssue issue) {
    impl->lexicalIssues.push_back(std::move(issue));
}

void IssueHandler::push(lex::LexicalIssue issue, Fatal) {
    push(std::move(issue));
    setFatal();
}

void IssueHandler::push(sema::SemanticIssue issue) {
    impl->semaIssues.push_back(std::move(issue));
}

void IssueHandler::push(sema::SemanticIssue issue, Fatal) {
    push(std::move(issue));
    setFatal();
}

std::span<lex::LexicalIssue const> IssueHandler::lexicalIssues() const {
    return impl->lexicalIssues;
}

std::span<sema::SemanticIssue const> IssueHandler::semaIssues() const {
    return impl->semaIssues;
}

} // namespace scatha::issue
