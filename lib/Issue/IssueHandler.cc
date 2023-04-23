#include "Issue/IssueHandler.h"

#include <iostream>

using namespace scatha;

void IssueHandler::print(std::string_view source) { print(source, std::cout); }

void IssueHandler::print(std::string_view source, std::ostream& ostream) {
    for (auto* issue: *this) {
        issue->print(source, ostream);
    }
}
