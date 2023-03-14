#include "Sema/FunctionSignature.h"

#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/utility.hpp>

namespace scatha::sema {

u64 FunctionSignature::hashArguments(std::span<TypeID const> types) {
    auto r =
        types | ranges::views::transform([](TypeID x) { return x.hash(); });
    return utl::hash_combine_range(r.begin(), r.end());
}

u64 FunctionSignature::computeTypeHash(TypeID returnTypeID, u64 argumentHash) {
    return utl::hash_combine(returnTypeID.hash(), argumentHash);
}

} // namespace scatha::sema
