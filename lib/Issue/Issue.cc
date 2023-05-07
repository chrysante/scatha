#include "Issue/Issue.h"

#include <iostream>

#include "Issue/Format.h"

using namespace scatha;

void Issue::print(std::string_view source) const { print(source, std::cout); }

void Issue::print(std::string_view source, std::ostream& str) const {
    str << "Error at L:" << sourceLocation().line
        << " C:" << sourceLocation().column << " : " << message() << "\n";
    highlightSource(source, sourceRange(), str);
}
