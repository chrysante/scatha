#include "Issue/Issue.h"

#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/streammanip.hpp>

#include "Issue/Format.h"

using namespace scatha;
using namespace tfmt::modifiers;

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
                                              SourceStructureMap& sourceMap,
                                              SourceLocation loc) {
    auto format = tfmt::FormatGuard(BrightGrey);
    size_t index = loc.fileIndex;
    auto name = sourceMap(index).name();
    if (!name.empty()) {
        str << name << " ";
    }
    else {
        str << "File " << index + 1 << " ";
    }
    if (!loc.valid()) {
        return;
    }
    str << "Line: " << loc.line << ", "
        << "Column: " << loc.column << " ";
});

void Issue::print(SourceStructureMap& sourceMap, std::ostream& str) const {
    /// The old issue highlight style
    if (highlights.empty()) {
        str << label(severity());
        format(str);
        str << " " << location(sourceMap, sourceLocation());
        str << "\n";
        if (sourceRange().valid()) {
            highlightSource(sourceMap, sourceRange(), severity(), str);
        }
    }
    /// New style
    else {
        str << label(severity());
        if (_header) {
            str << tfmt::format(BrightBlue | Italic, _header) << " ";
        }
        str << location(sourceMap, sourceLocation());
        str << "\n";
        highlightSource(sourceMap, highlights, severity(), str);
        if (_hint) {
            str << tfmt::format(Green | Bold, "Hint: ") << _hint << "\n";
        }
    }
}

void Issue::print(SourceStructureMap& sourceMap) const {
    print(sourceMap, std::cout);
}

void Issue::print(std::string_view source, std::ostream& ostream) const {
    auto file = SourceFile::make(std::string(source));
    SourceStructureMap sourceMap(std::span(&file, 1));
    print(sourceMap, ostream);
}

void Issue::print(std::string_view source) const { print(source, std::cout); }
