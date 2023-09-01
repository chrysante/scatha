// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_UNIQUEPTR_H_
#define SCATHA_COMMON_UNIQUEPTR_H_

#include <compare>
#include <memory>

#include <scatha/Common/Base.h>

namespace scatha::internal {

template <typename T>
void privateDelete(T* ptr) {
    static_assert(sizeof(T), "T is incomplete");
    delete ptr;
}

template <typename T>
void privateDestroy(T* ptr) {
    std::destroy_at(ptr);
}

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

} // namespace scatha

#endif // SCATHA_COMMON_UNIQUEPTR_H_
