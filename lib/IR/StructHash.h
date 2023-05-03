#ifndef SCATHA_IR_STRUCTHASH_H_
#define SCATHA_IR_STRUCTHASH_H_

/// The classes in this file are used to make structure types unique

#include <range/v3/algorithm.hpp>
#include <utl/hash.hpp>
#include <utl/vector.hpp>

#include <scatha/IR/Fwd.h>

namespace scatha::ir::internal {

struct StructHash {
    using is_transparent = void;

    size_t operator()(std::span<Type const* const> members) const {
        return utl::hash_combine_range(members.begin(), members.end());
    }

    size_t operator()(utl::vector<Type const*> const& members) const {
        return (*this)(std::span<Type const* const>(members));
    }
};

struct StructEqual {
    using is_transparent = void;

    bool operator()(utl::vector<Type const*> const& a,
                    utl::vector<Type const*> const& b) const {
        return ranges::equal(a, b);
    }

    bool operator()(utl::vector<Type const*> const& a,
                    std::span<Type const* const> b) const {
        return ranges::equal(a, b);
    }
};

} // namespace scatha::ir::internal

#endif // SCATHA_IR_STRUCTHASH_H_
