#include "Sema/OverloadSet.h"

#include <range/v3/view.hpp>

namespace scatha::sema {

Function const* OverloadSet::find(
    std::span<Type const* const> argumentTypes) const {
    auto matches =
        functions | ranges::views::filter([&](Function* F) {
            return ranges::equal(argumentTypes, F->signature().argumentTypes());
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

} // namespace scatha::sema
