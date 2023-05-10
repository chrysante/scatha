#include "Issue/Issue.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Issue/Format.h"

using namespace scatha;

void Issue::print(std::string_view source) const { print(source, std::cout); }

static constexpr utl::streammanip label([](std::ostream& str,
                                           IssueSeverity severity) {
    switch (severity) {
    case IssueSeverity::Error:
        str << tfmt::format(tfmt::Red | tfmt::Bold, "Error: ");
        break;
    case IssueSeverity::Warning:
        str << tfmt::format(tfmt::Yellow | tfmt::Bold, "Warning: ");
        break;
    }
});

void Issue::print(std::string_view source, std::ostream& str) const {
    str << label(severity()) << "L:" << sourceLocation().line
        << " C:" << sourceLocation().column << " : " << message() << "\n";
    highlightSource(source, sourceRange(), str);
}
