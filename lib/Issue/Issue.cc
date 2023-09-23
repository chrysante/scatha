#include "Issue/Issue.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Issue/Format.h"

using namespace scatha;
using namespace tfmt::modifiers;

void Issue::print(SourceStructure const& source) const {
    print(source, std::cout);
}

static constexpr utl::streammanip label([](std::ostream& str,
                                           IssueSeverity severity) {
    switch (severity) {
    case IssueSeverity::Error:
        str << tfmt::format(Red | Bold, "Error: ");
        break;
    case IssueSeverity::Warning:
        str << tfmt::format(Yellow | Bold, "Warning: ");
        break;
    }
});

static constexpr utl::streammanip location([](std::ostream& str,
                                              SourceLocation loc) {
    str << tfmt::format(BrightGrey, "Line: ") << loc.line << " "
        << tfmt::format(BrightGrey, "Column: ") << loc.column << " ";
});

void Issue::print(SourceStructure const& source, std::ostream& str) const {
    str << label(severity());
    if (sourceLocation().valid()) {
        str << location(sourceLocation());
    }
    if (highlights.empty()) {
        format(str);
        str << "\n";
        if (sourceRange().valid()) {
            highlightSource(source, sourceRange(), str);
        }
    }
    else {
        if (_header) {
            str << tfmt::format(BrightBlue | Italic, _header);
        }
        str << "\n";
        highlightSource(source, highlights, str);
        if (_hint) {
            str << tfmt::format(Green | Bold, "Hint: ") << _hint << "\n";
        }
    }
}
