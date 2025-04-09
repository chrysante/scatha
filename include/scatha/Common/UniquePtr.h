#ifndef SCATHA_COMMON_UNIQUEPTR_H_
#define SCATHA_COMMON_UNIQUEPTR_H_

#include <compare>
#include <memory>

#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha {

template <typename T>
using UniquePtr = csp::unique_ptr<T>;

template <typename T, typename... Args>
    requires std::constructible_from<T, Args...>
UniquePtr<T> allocate(Args&&... args) {
    return csp::make_unique<T>(std::forward<Args>(args)...);
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
