#ifndef SCATHA_IR_VECTORHASH_H_
#define SCATHA_IR_VECTORHASH_H_

/// The classes in this file are used to make structure types unique

#include <range/v3/algorithm.hpp>
#include <utl/hash.hpp>
#include <utl/vector.hpp>

#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Hash object that computes hashes of `utl::vector` to be used as hash
/// functions in containers
template <typename T>
struct VectorHash {
    using is_transparent = void;

    size_t operator()(std::span<T const> elems) const {
        return utl::hash_combine_range(elems.begin(), elems.end());
    }

    size_t operator()(utl::vector<T> const& elems) const {
        return (*this)(std::span<T const>(elems));
    }
};

/// Function object that compares objects of `utl::vector` for equality. To be
/// used as compare functions in containers
template <typename T>
struct VectorEqual {
    using is_transparent = void;

    bool operator()(utl::vector<T> const& a, utl::vector<T> const& b) const {
        return ranges::equal(a, b);
    }

    bool operator()(utl::vector<T> const& a, std::span<T const> b) const {
        return ranges::equal(a, b);
    }
};

} // namespace scatha::ir

#endif // SCATHA_IR_VECTORHASH_H_
