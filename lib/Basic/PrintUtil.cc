#include "Basic/PrintUtil.h"

#include <ostream>

namespace scatha {

std::ostream &operator<<(std::ostream &str, Indenter const &ind) {
    for (int i = 0; i < ind.level * ind.spacesPerLevel; ++i) {
        str << ' ';
    }
    return str;
}

std::ostream &operator<<(std::ostream &str, EndlIndenter const &endl) {
    return str << '\n' << static_cast<Indenter const &>(endl);
}

} // namespace scatha
