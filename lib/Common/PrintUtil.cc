#include "Common/PrintUtil.h"

#include <ostream>

std::ostream& scatha::operator<<(std::ostream& str, Indenter const& ind) {
    for (int i = 0; i < ind.level() * ind.spacesPerLevel(); ++i) {
        str << ' ';
    }
    return str;
}

std::ostream& scatha::operator<<(std::ostream& str, EndlIndenter const& endl) {
    return str << '\n' << static_cast<Indenter const&>(endl);
}

utl::vstreammanip<> scatha::tableBegin(int border,
                                       int cellborder,
                                       int cellspacing) {
    return [=](std::ostream& str) {
        str << "<table border=\"" << border << "\" cellborder=\"" << cellborder
            << "\" cellspacing=\"" << cellspacing << "\">\n";
    };
}

utl::vstreammanip<> scatha::tableEnd() {
    return [](std::ostream& str) { str << "</table>\n"; };
}

utl::vstreammanip<> scatha::fontBegin(std::string fontname) {
    return [=](std::ostream& str) {
        str << "<font face=\"" << fontname << "\">\n";
    };
}

utl::vstreammanip<> scatha::fontEnd() {
    return [](std::ostream& str) { str << "</font>\n"; };
}

utl::vstreammanip<> scatha::rowBegin() {
    return [](std::ostream& str) { str << "<tr><td align=\"left\">\n"; };
}

utl::vstreammanip<> scatha::rowEnd() {
    return [](std::ostream& str) { str << "</td></tr>\n"; };
}
