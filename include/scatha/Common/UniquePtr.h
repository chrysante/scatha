// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_UNIQUEPTR_H_
#define SCATHA_COMMON_UNIQUEPTR_H_

#include <compare>
#include <memory>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

namespace scatha::internal {

struct PrivateDeleter {
    void operator()(auto* ptr) const { privateDelete(ptr); }
};

} // namespace scatha::internal

namespace scatha {

template <typename T>
using UniquePtr = std::unique_ptr<T, internal::PrivateDeleter>;

template <typename T, typename... Args>
    requires std::constructible_from<T, Args...>
UniquePtr<T> allocate(Args&&... args) {
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

template <typename Derived, typename Base>
    requires std::derived_from<Derived, Base>
UniquePtr<Derived> uniquePtrCast(UniquePtr<Base>&& p) {
    auto* d = cast<Derived*>(p.release());
    return UniquePtr<Derived>(d);
}

/// Utility function to convert `UniquePtr` arguments into a
/// `small_vector<UniquePtr>`
template <typename... T>
utl::small_vector<UniquePtr<std::common_type_t<T...>>> toSmallVector(
    UniquePtr<T>... ptrs) {
    utl::small_vector<UniquePtr<std::common_type_t<T...>>> result;
    (result.push_back(std::move(ptrs)), ...);
    return result;
}

} // namespace scatha

#endif // SCATHA_COMMON_UNIQUEPTR_H_
