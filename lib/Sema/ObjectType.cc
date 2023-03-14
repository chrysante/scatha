#include "Sema/ObjectType.h"

namespace scatha::sema {

bool ObjectType::isComplete() const {
    SC_ASSERT((_size == invalidSize) == (_align == invalidSize),
              "Either both or neither must be invalid");
    return (_size != invalidSize);
}

} // namespace scatha::sema
