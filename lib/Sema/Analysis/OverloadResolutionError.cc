#include "Sema/Analysis/OverloadResolutionError.h"

#include <ostream>

#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

OverloadResolutionError::OverloadResolutionError(
    OverloadSet const* overloadSet):
    SemanticIssue(SourceLocation{}, IssueSeverity::Error), os(overloadSet) {}

void NoMatchingFunction::format(std::ostream& str) const {
    str << "No matching function for call to '" << overloadSet()->name() << "'";
}

void AmbiguousOverloadResolution::format(std::ostream& str) const {
    str << "Ambiguous call to '" << overloadSet()->name() << "'";
}
