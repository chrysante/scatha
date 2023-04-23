#include "Sema/Type.h"

namespace scatha::sema {

bool Type::isComplete() const {
    SC_ASSERT((size() == invalidSize) == (align() == invalidSize),
              "Either both or neither must be invalid");
    return size() != invalidSize;
}

} // namespace scatha::sema
