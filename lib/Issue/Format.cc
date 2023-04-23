#include "Issue/Format.h"

#include <iomanip>
#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

using namespace scatha;

void scatha::highlightSource(std::string_view text,
                             SourceLocation sourceLoc,
                             size_t numChars) {
    highlightSource(text, sourceLoc, numChars, std::cout);
}

static std::string_view getLineImpl(std::string_view text, size_t index) {
    size_t begin = index;
    if (text[begin] != '\n') {
        while (true) {
            if (begin == 0) {
                break;
            }
            if (text[begin] == '\n') {
                ++begin;
                break;
            }
            --begin;
        }
    }
    size_t end = index;
    while (end < text.size() && text[end] != '\n') {
        ++end;
    }
    return std::string_view(text.data() + begin, end - begin);
}

static std::string_view getLine(std::string_view text,
                                size_t index,
                                ssize_t offset = 0) {
    while (offset < 0) {
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
            ++offset;
        } while (text[index] == '\n' && offset != 0);
    }
    while (offset > 0) {
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
            --offset;
        } while (text[index] == '\n' && offset != 0);
    }
    return getLineImpl(text, index);
}

static auto lineNumber(ssize_t index) {
    return tfmt::format(tfmt::brightGrey,
                        utl::strcat(std::setw(4), index, ": "));
}

void scatha::highlightSource(std::string_view text,
                             SourceLocation sourceLocation,
                             size_t numChars,
                             std::ostream& str) {
    size_t const index  = utl::narrow_cast<size_t>(sourceLocation.index);
    size_t const column = utl::narrow_cast<size_t>(sourceLocation.column);
    std::string_view const line = getLine(text, index);
    numChars                    = std::min(numChars, line.size() - column + 1);

    /// Previous lines
    ssize_t lineIndex = std::max(0, sourceLocation.line - 3);
    for (; lineIndex != sourceLocation.line; ++lineIndex) {
        str << lineNumber(lineIndex)
            << getLine(text, index, lineIndex - sourceLocation.line) << "\n";
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
    for (; lineIndex != sourceLocation.line + 3; ++lineIndex) {
        str << lineNumber(lineIndex)
            << getLine(text, index, lineIndex - sourceLocation.line) << "\n";
    }
}
