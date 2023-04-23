#include "Sema/Fwd.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(EntityType t) {
    return std::array{
#define SC_SEMA_ENTITY_DEF(Type, _) std::string_view(#Type),
#include "Sema/Lists.def"
    }[static_cast<size_t>(t)];
}

std::ostream& sema::operator<<(std::ostream& str, EntityType t) {
    return str << toString(t);
}
