#include "Issue/IssueHandler.h"

#include <iostream>

#include <range/v3/algorithm.hpp>

using namespace scatha;

bool IssueHandler::haveErrors() const {
    return ranges::any_of(*this, [](auto* issue) { return issue->isError(); });
}

void IssueHandler::print(std::string_view source) { print(source, std::cout); }

void IssueHandler::print(std::string_view source, std::ostream& ostream) {
    for (auto* issue: *this) {
        issue->print(source, ostream);
    }
}
