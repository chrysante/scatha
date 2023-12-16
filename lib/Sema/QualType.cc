#include "Sema/QualType.h"

#include <utl/strcat.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

std::string QualType::qualName() const {
    if (!get()) {
        return "NULL";
    }
    if (isMut()) {
        return utl::strcat("mut ", get()->name());
    }
    return std::string(get()->name());
}
