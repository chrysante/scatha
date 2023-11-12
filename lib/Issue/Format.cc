#include "Issue/Format.h"

#include <iomanip>
#include <iostream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>
#include <utl/utility.hpp>

#include "Common/Base.h"
#include "Issue/Message.h"
#include "Issue/SourceStructure.h"

using namespace scatha;
using namespace tfmt::modifiers;

static std::vector<std::string_view> splitText(std::string_view text,
                                               char delim) {
    return ranges::views::split(text, delim) |
           ranges::views::transform(
               [](auto&& r) {
        return std::string_view(&*r.begin(),
                                utl::narrow_cast<size_t>(ranges::distance(r)));
           }) |
        ranges::to<std::vector>;
}

SourceStructure::SourceStructure(std::string filename, std::string_view text):
    _name(std::move(filename)), source(text), lines(splitText(text, '\n')) {}

IssueMessage::IssueMessage(std::string msg):
    IssueMessage([=](std::ostream& str) { str << msg; }) {}

static constexpr size_t LineNumChars = 6;

static constexpr utl::streammanip blank =
    [](std::ostream& str, size_t numChars) {
    for (size_t i = 0; i < numChars; ++i) {
        str << ' ';
    }
};

static constexpr utl::streammanip lineNumber =
    [](std::ostream& str, ssize_t index) {
    if (index >= 0) {
        /// \Warning This must print exactly `LineNumChars` many characters
        str << tfmt::format(BrightGrey, utl::strcat(std::setw(4), index, ": "));
    }
    else {
        str << blank(LineNumChars);
    }
};

static constexpr utl::streammanip highlightLineRange =
    [](std::ostream& str, std::string_view text, size_t begin, size_t end) {
    str << tfmt::format(BrightGrey, text.substr(0, begin));
    str << tfmt::format(None, text.substr(begin, end - begin));
    str << tfmt::format(BrightGrey, text.substr(end, text.size() - end));
};

static constexpr utl::streammanip squiggle =
    [](std::ostream& str, tfmt::Modifier mod, size_t numChars) {
    tfmt::format(mod, [&] {
        for (size_t i = 0; i < std::max(size_t{ 1 }, numChars); ++i) {
#if SC_UNICODE_TERMINAL
            str << "˜";
#else
            str << "^";
#endif
        }
    });
};

namespace {

struct SrcHighlightCtx {
    std::ostream& str;
    SourceStructureMap& sourceMap;
    std::vector<SourceHighlight>& highlights;
    IssueSeverity severity;

    SrcHighlightCtx(std::ostream& str,
                    SourceStructureMap& sourceMap,
                    std::vector<SourceHighlight>& highlights,
                    IssueSeverity severity):
        str(str),
        sourceMap(sourceMap),
        highlights(highlights),
        severity(severity) {}

    void run();

    void printHighlightLine(SourceHighlight const& highlight,
                            SourceStructure const& source);

    void printMessage(size_t currentColumn, SourceHighlight const& highlight);

    void printLines(int index, int count, SourceStructure const& source);
};

} // namespace

void scatha::highlightSource(SourceStructureMap& source,
                             SourceRange sourceRange,
                             IssueSeverity severity,
                             std::ostream& str) {
    highlightSource(source,
                    { { HighlightKind::Primary, sourceRange, {} } },
                    severity,
                    str);
}

void scatha::highlightSource(SourceStructureMap& source,
                             std::vector<SourceHighlight> highlights,
                             IssueSeverity severity,
                             std::ostream& str) {
    SrcHighlightCtx(str, source, highlights, severity).run();
}

void scatha::highlightSource(SourceStructureMap& source,
                             SourceRange sourceRange,
                             IssueSeverity severity) {
    highlightSource(source, sourceRange, severity, std::cout);
}

static int lineProj(SourceHighlight& h) { return h.position.begin().line - 1; }

void SrcHighlightCtx::run() {
    int const PaddingLines = 1;
    for (auto& H: highlights) {
        auto& source = sourceMap(H.position.begin().fileIndex);
        int line = lineProj(H);
        if (line >= source.size()) {
            continue;
        }
        /// Begin padding lines
        printLines(line - PaddingLines, PaddingLines, source);
        ///
        printHighlightLine(H, source);
        /// End padding lines
        printLines(line + 1, PaddingLines, source);
    }
}

static auto toMod(IssueSeverity severity) {
    using enum IssueSeverity;
    switch (severity) {
    case Warning:
        return Yellow | Bold;
    case Error:
        return Red | Bold;
    }
}

void SrcHighlightCtx::printHighlightLine(SourceHighlight const& highlight,
                                         SourceStructure const& source) {
    auto range = highlight.position;
    SC_EXPECT(range.valid());
    auto const sl = range.begin();
    int const line = sl.line - 1;
    int const column = sl.column - 1;
    size_t const ucolumn = utl::narrow_cast<size_t>(column);
    auto lineText = source[line];
    size_t const endcol = range.begin().line == range.end().line ?
                              utl::narrow_cast<size_t>(range.end().column - 1) :
                              lineText.size();
    str << lineNumber(sl.line)
        << highlightLineRange(source[line], ucolumn, endcol) << "\n";
    str << lineNumber(-1) << blank(column)
        << squiggle(toMod(severity), endcol - ucolumn);
    printMessage(endcol + LineNumChars, highlight);
    str << "\n";
}

static size_t wordlength(std::string_view word) {
    size_t size = 0;
    for (size_t i = 0; i < word.size(); ++i) {
        if (word[i] == '\033') {
            while (i < word.size() && word[i] != 'm') {
                ++i;
            }
            continue;
        }
        ++size;
    }
    return size;
}

void SrcHighlightCtx::printMessage(size_t currentColumn,
                                   SourceHighlight const& highlight) {
    std::stringstream sstr;
    tfmt::copyFormatFlags(str, sstr);
    auto fmtFlags =
        highlight.kind == HighlightKind::Primary ? Italic : BrightGrey | Italic;
    sstr << tfmt::format(fmtFlags, highlight.message);
    auto text = std::move(sstr).str();
    auto words = splitText(text, ' ');
    size_t width = tfmt::getWidth(str).value_or(80);
    for (auto word: words) {
        size_t size = wordlength(word);
        currentColumn += size;
        if (currentColumn >= width) {
            currentColumn = LineNumChars + size;
            str << '\n' << blank(LineNumChars);
        }
        else {
            str << ' ';
            ++currentColumn;
        }
        str << word;
    }
}

void SrcHighlightCtx::printLines(int userIdx,
                                 int count,
                                 SourceStructure const& source) {
    int index = std::clamp(userIdx, 0, int(source.size()));
    int end = std::clamp(userIdx + count, 0, int(source.size()));
    for (; index < end; ++index) {
        str << lineNumber(index + 1) << tfmt::format(BrightGrey, source[index])
            << "\n";
    }
}
