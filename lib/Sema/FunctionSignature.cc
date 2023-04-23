#include "Sema/FunctionSignature.h"

#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/utility.hpp>

#include "Sema/QualType.h"
#include "Sema/Type.h"

using namespace scatha;
using namespace sema;

static TypeID toTypeID(QualType const* type) {
    return type ? type->base()->symbolID() : TypeID::Invalid;
}

u64 FunctionSignature::hashArguments(std::span<QualType const* const> types) {
    auto r = types | ranges::views::transform([](QualType const* type) {
                 return toTypeID(type).hash();
             });
    return utl::hash_combine_range(r.begin(), r.end());
}

u64 FunctionSignature::computeTypeHash(QualType const* returnType,
                                       u64 argumentHash) {
    return utl::hash_combine(toTypeID(returnType).hash(), argumentHash);
}
