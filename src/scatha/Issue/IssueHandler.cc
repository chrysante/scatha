#include "Issue/IssueHandler.h"

#include <iostream>

#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>

#include "Issue/Format.h"

using namespace scatha;

bool IssueHandler::haveErrors() const {
    return ranges::any_of(*this, [](auto* issue) { return issue->isError(); });
}

void IssueHandler::print(std::span<SourceFile const> files,
                         std::ostream& str) const {
    SourceStructureMap structureMap(files);
    for (auto* issue: *this) {
        issue->print(structureMap, str);
        str << "\n";
    }
}

void IssueHandler::print(std::span<SourceFile const> files) const {
    print(files, std::cout);
}

void IssueHandler::print(std::string_view text, std::ostream& str) const {
    auto source = SourceFile::make(std::string(text));
    print(std::span(&source, 1), str);
}

void IssueHandler::print(std::string_view text) const {
    print(text, std::cout);
}
