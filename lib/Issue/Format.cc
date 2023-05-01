#include "Issue/Format.h"

#include <iomanip>
#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

using namespace scatha;

void scatha::highlightSource(std::string_view text, SourceRange sourceRange) {
    highlightSource(text, sourceRange, std::cout);
}

static std::string_view getLineImpl(std::string_view text, size_t index) {
    size_t begin = index;
    if (text[begin] != '\n') {
        while (true) {
            if (text[begin] == '\n') {
                ++begin;
                break;
            }
            if (begin == 0) {
                break;
            }
            --begin;
        }
    }
    else if (begin + 1 < text.size() && text[begin + 1] != '\n') {
        ++begin;
    }
    size_t end = index;
    while (end < text.size() && text[end] != '\n') {
        ++end;
    }
    return std::string_view(text.data() + begin, end - begin);
}

static std::string_view getLine(std::string_view text,
                                size_t index,
                                ssize_t lineOffset = 0) {
    while (lineOffset < 0) {
        while (text[index] != '\n') {
            if (index == 0) {
                return {};
            }
            --index;
        }
        do {
            if (index == 0) {
                return {};
            }
            --index;
            ++lineOffset;
        } while (text[index] == '\n' && lineOffset != 0);
    }
    while (lineOffset > 0) {
        while (text[index] != '\n') {
            ++index;
            if (index == text.size()) {
                return {};
            }
        }
        do {
            ++index;
            if (index == text.size()) {
                return {};
            }
            --lineOffset;
        } while (text[index] == '\n' && lineOffset != 0);
    }
    return getLineImpl(text, index);
}

static auto lineNumber(ssize_t index) {
    return tfmt::format(tfmt::brightGrey,
                        utl::strcat(std::setw(4), index, ": "));
}

void scatha::highlightSource(std::string_view text,
                             SourceRange sourceRange,
                             std::ostream& str) {
    size_t const index  = utl::narrow_cast<size_t>(sourceRange.begin().index);
    size_t const column = utl::narrow_cast<size_t>(sourceRange.begin().column);
    std::string_view const line = getLine(text, index);
    size_t const numChars =
        sourceRange.begin().line == sourceRange.end().line ?
            utl::narrow_cast<size_t>(sourceRange.end().column -
                                     sourceRange.begin().column) :
            line.size() - column + 1;
    SourceLocation const beginLoc = sourceRange.begin();

    /// Previous lines
    ssize_t lineIndex = std::max(0, beginLoc.line - 3);
    for (; lineIndex != beginLoc.line; ++lineIndex) {
        str << lineNumber(lineIndex)
            << getLine(text, index, lineIndex - beginLoc.line) << "\n";
    }

    /// Erroneous line
    str << lineNumber(lineIndex);
    str << line.substr(0, column - 1);
    str << tfmt::format(tfmt::red | tfmt::italic,
                        line.substr(column - 1, numChars));
    size_t const endPos = column - 1 + numChars;
    str << line.substr(endPos, line.size() - endPos);
    str << '\n';
    ++lineIndex;

    /// Squiggly line
    for (size_t i = 0; i < 6 + column - 1; ++i) {
        str << ' ';
    }
    tfmt::format(tfmt::red, [&] {
        for (size_t i = 0; i < std::max(size_t{ 1 }, numChars); ++i) {
            str << "˜";
        }
    });
    str << "\n";

    /// Next lines
    for (; lineIndex != beginLoc.line + 3; ++lineIndex) {
        str << lineNumber(lineIndex)
            << getLine(text, index, lineIndex - beginLoc.line) << "\n";
    }
}
