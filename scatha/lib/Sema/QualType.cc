#include "Sema/QualType.h"

#include <sstream>

#include <utl/strcat.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

std::string QualType::qualName() const {
    if (!get()) {
        return "NULL";
    }
    std::stringstream sstr;
    if (isDyn()) {
        sstr << "dyn ";
    }
    if (isMut()) {
        sstr << "mut ";
    }
    sstr << get()->name();
    return std::move(sstr).str();
}
