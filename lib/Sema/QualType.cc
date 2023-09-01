#include "Sema/QualType.h"

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

std::string QualType::name() const {
    return isMutable() ? "mut " + std::string(get()->name()) :
                         std::string(get()->name());
}
