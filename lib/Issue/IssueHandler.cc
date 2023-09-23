#include "Issue/IssueHandler.h"

#include <iostream>

#include <range/v3/algorithm.hpp>

using namespace scatha;

bool IssueHandler::haveErrors() const {
    return ranges::any_of(*this, [](auto* issue) { return issue->isError(); });
}

void IssueHandler::print(std::string_view source) const {
    print(source, std::cout);
}

void IssueHandler::print(std::string_view text, std::ostream& ostream) const {
    SourceStructure source(text);
    for (auto* issue: *this) {
        issue->print(source, ostream);
        ostream << "\n";
    }
}
