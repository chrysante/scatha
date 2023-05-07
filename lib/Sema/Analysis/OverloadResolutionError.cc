#include "Sema/Analysis/OverloadResolutionError.h"

#include <utl/strcat.hpp>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

OverloadResolutionError::OverloadResolutionError(
    OverloadSet const* overloadSet):
    SemanticIssue(SourceLocation{}, IssueSeverity::Error), os(overloadSet) {}

std::string NoMatchingFunction::message() const {
    return utl::strcat("No matching function for call to '",
                       overloadSet()->name(),
                       "'");
}

std::string AmbiguousOverloadResolution::message() const {
    return utl::strcat("Ambiguous call to '", overloadSet()->name(), "'");
}
