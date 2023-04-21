#include "AST/SourceLocation.h"

#include <iomanip>
#include <ostream>

namespace scatha {

std::ostream& operator<<(std::ostream& str, SourceLocation const& sc) {
    return str << "(" << std::setw(3) << sc.line << ", " << std::setw(3)
               << sc.column << ")";
}

} // namespace scatha
