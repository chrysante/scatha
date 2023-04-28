#include "Sema/OverloadSet.h"

#include <range/v3/view.hpp>

#include "Sema/QualType.h"

using namespace scatha;
using namespace sema;

static bool signatureMatch(std::span<QualType const* const> parameterTypes,
                           std::span<QualType const* const> argumentTypes) {
    if (parameterTypes.size() != argumentTypes.size()) {
        return false;
    }
    for (auto [param, arg]: ranges::views::zip(parameterTypes, argumentTypes)) {
        if (param->base() != arg->base()) {
            return false;
        }
        SC_ASSERT(!param->has(TypeQualifiers::ExplicitReference), "");
        if (param->has(TypeQualifiers::ImplicitReference) &&
            !arg->has(TypeQualifiers::ExplicitReference))
        {
            return false;
        }
        if (!param->has(TypeQualifiers::ImplicitReference) &&
            arg->has(TypeQualifiers::ExplicitReference))
        {
            return false;
        }
    }
    return true;
}

Function const* OverloadSet::find(
    std::span<QualType const* const> argumentTypes) const {
    auto matches = functions | ranges::views::filter([&](Function* F) {
                       return signatureMatch(F->signature().argumentTypes(),
                                             argumentTypes);
                   }) |
                   ranges::to<utl::small_vector<Function*>>;
    if (matches.size() == 1) {
        return matches.front();
    }
    return nullptr;
}

std::pair<Function const*, bool> OverloadSet::add(Function* F) {
    SC_ASSERT(F->name() == name(),
              "Name of function must match name of overload set");
    auto const [itr, success] = funcSet.insert(F);
    if (success) {
        functions.push_back(F);
    }
    return { *itr, success };
}
