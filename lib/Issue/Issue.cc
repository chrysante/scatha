#include "Issue/Issue.h"

#include <iostream>

#include "Issue/Format.h"

using namespace scatha;

void Issue::print(std::string_view source) { print(source, std::cout); }

void Issue::print(std::string_view source, std::ostream& str) {
    str << message() << "\n";
    highlightSource(source, sourceLocation(), 3, str);
}
