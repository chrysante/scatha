#include "AST/SourceLocation.h"

#include <iomanip>
#include <ostream>

using namespace scatha;

std::ostream& scatha::operator<<(std::ostream& str, SourceLocation const& sc) {
    return str << "(" << std::setw(3) << sc.line << ", " << std::setw(3)
               << sc.column << ")";
}

SourceRange scatha::merge(SourceRange lhs, SourceRange rhs) {
    if (!lhs.valid()) {
        return rhs;
    }
    if (!rhs.valid()) {
        return lhs;
    }
    return {
        lhs.begin() < rhs.begin() ? lhs.begin() : rhs.begin(),
        lhs.end() > rhs.end() ? lhs.end() : rhs.end(),
    };
}
