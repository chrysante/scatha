#include "Issue/Format.h"

#include <iostream>

#include <utl/stdio.hpp>

using namespace scatha;

void issue::highlightToken(StructuredSource const& source, Token const& token) {
    highlightToken(source, token, std::cout);
}

void issue::highlightToken(StructuredSource const& source,
                           Token const& token,
                           std::ostream& str) {
#if 0
    auto const sourceLocation   = token.sourceLocation;
    std::string_view const line = source.getLine(sourceLocation.line - 1);
    str << line.substr(0, sourceLocation.column - 1);
    str << utl::format("{0}{2}{1}",
                       utl::format_codes::_private::_decay(utl::format_codes::red, true),
                       utl::format_codes::_private::_decay(utl::format_codes::reset, true),
                       line.substr(sourceLocation.column - 1, token.id.size()));
    size_t const endPos = sourceLocation.column - 1 + token.id.size();
    str << line.substr(endPos, line.size() - endPos);
    str << '\n';
    str << utl::format("{0: >{1}}{0:^>{2}}\n", "", sourceLocation.column - 1, std::max(size_t{ 1 }, token.id.size()));
#endif
}
