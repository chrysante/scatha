#include "Sema/Analysis/OverloadResolution.h"

#include <range/v3/view.hpp>

#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

static bool signatureMatch(std::span<QualType const* const> parameterTypes,
                           std::span<QualType const* const> argumentTypes) {
    if (parameterTypes.size() != argumentTypes.size()) {
        return false;
    }
    for (auto [param, arg]: ranges::views::zip(parameterTypes, argumentTypes)) {
        if (!param || !arg) {
            return false;
        }
        if (!isImplicitlyConvertible(arg, param)) {
            return false;
        }
    }
    return true;
}

Expected<Function*, OverloadResolutionError*> performOverloadResolution(
    OverloadSet* overloadSet, std::span<QualType const* const> argumentTypes) {
    auto matches = *overloadSet | ranges::views::filter([&](Function* F) {
        return signatureMatch(F->argumentTypes(), argumentTypes);
    }) | ranges::to<utl::small_vector<Function*>>;
    switch (matches.size()) {
    case 0:
        return new NoMatchingFunction(overloadSet);

    case 1:
        return matches.front();

    default:
        return new AmbiguousOverloadResolution(overloadSet, std::move(matches));
    }
}
