#include "Issue/Format.h"

#include <iomanip>
#include <iostream>

#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>
#include <utl/utility.hpp>

#include "Common/Base.h"

using namespace scatha;
using namespace tfmt::modifiers;

static std::vector<std::string_view> splitLines(std::string_view text) {
    return ranges::views::split(text, '\n') |
           ranges::views::transform(
               [](auto&& r) {
        return std::string_view(&*r.begin(),
                                utl::narrow_cast<size_t>(ranges::distance(r)));
           }) |
        ranges::to<std::vector>;
}

SourceStructure::SourceStructure(std::string_view text):
    source(text), lines(splitLines(text)) {}

void scatha::highlightSource(SourceStructure const& source,
                             SourceRange sourceRange) {
    highlightSource(source, sourceRange, std::cout);
}

static constexpr utl::streammanip lineNumber =
    [](std::ostream& str, ssize_t index) {
    if (index >= 0) {
        str << tfmt::format(BrightGrey, utl::strcat(std::setw(4), index, ": "));
    }
    else {
        str << tfmt::format(None, "      ");
    }
};

static constexpr utl::streammanip highlightLineRange =
    [](std::ostream& str, std::string_view text, size_t begin, size_t end) {
    str << text.substr(0, begin);
    str << tfmt::format(Bold, text.substr(begin, end - begin));
    str << text.substr(end, text.size() - end);
};

static constexpr utl::streammanip blank =
    [](std::ostream& str, size_t numChars) {
    for (size_t i = 0; i < numChars; ++i) {
        str << ' ';
    }
};

static constexpr utl::streammanip squiggle =
    [](std::ostream& str, size_t numChars) {
    tfmt::format(Red | Bold, [&] {
        for (size_t i = 0; i < std::max(size_t{ 1 }, numChars); ++i) {
#if SC_UNICODE_TERMINAL
            str << "˜";
#else
            str << "^";
#endif
        }
    });
};

void scatha::highlightSource(SourceStructure const& source,
                             SourceRange sourceRange,
                             std::ostream& str) {
    SourceHighlight highlight{ sourceRange, {} };
    highlightSource(source, std::span(&highlight, 1), str);
}

namespace {

struct SrcHighlightCtx {
    std::ostream& str;
    SourceStructure const& source;
    std::span<SourceHighlight const> highlights;

    SrcHighlightCtx(std::ostream& str,
                    SourceStructure const& source,
                    std::span<SourceHighlight const> highlights):
        str(str), source(source), highlights(highlights) {}

    void run();

    void printErrorLine(SourceRange range, std::string_view message);

    void printLines(int index, int count);
};

} // namespace

void scatha::highlightSource(SourceStructure const& source,
                             std::span<SourceHighlight const> highlights,
                             std::ostream& str) {
    SrcHighlightCtx(str, source, highlights).run();
}

void SrcHighlightCtx::run() {
    auto range = highlights.front().position;
    SC_ASSERT(range.valid(), "");

    int line = range.begin().line - 1;

    /// Previous lines
    printLines(line - 3, 3);

    printErrorLine(range, "");

    /// Next lines
    printLines(line + 1, 3);
}

void SrcHighlightCtx::printErrorLine(SourceRange range,
                                     std::string_view message) {
    auto const sl = range.begin();
    int const line = sl.line - 1;
    int const column = sl.column - 1;
    size_t const ucolumn = utl::narrow_cast<size_t>(column);
    auto lineText = source[line];
    size_t const endcol = range.begin().line == range.end().line ?
                              utl::narrow_cast<size_t>(range.end().column - 1) :
                              lineText.size();

    str << lineNumber(line) << highlightLineRange(source[line], ucolumn, endcol)
        << "\n";
    str << lineNumber(-1) << blank(column) << squiggle(endcol - ucolumn) << " "
        << message << "\n";
}

void SrcHighlightCtx::printLines(int index, int count) {
    index = std::max(index, 0);
    int end = std::min(index + count, int(source.size()));
    for (; index < end; ++index) {
        str << lineNumber(index) << tfmt::format(BrightGrey, source[index])
            << "\n";
    }
}
