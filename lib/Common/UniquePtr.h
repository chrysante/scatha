// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_UNIQUEPTR_H_
#define SCATHA_COMMON_UNIQUEPTR_H_

#include <compare>
#include <memory>

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::internal {

void privateDelete(auto* ptr) {
    delete ptr;
}

void privateDestroy(auto* ptr) {
    std::destroy_at(ptr);
}

} // namespace scatha::internal

namespace scatha {

/// ** Smart pointer  **
template <typename T>
class UniquePtr {
public:
    UniquePtr(): UniquePtr(nullptr) {}
    explicit UniquePtr(T* ptr): ptr(ptr) {}

    UniquePtr(std::nullptr_t): ptr(nullptr) {}

    template <typename U>
    requires std::convertible_to<U*, T*>
    UniquePtr(UniquePtr<U>&& rhs) noexcept: ptr(rhs.release()) {}

    UniquePtr(UniquePtr&& rhs) noexcept: ptr(rhs.release()) {}

    UniquePtr& operator=(UniquePtr&& rhs) & noexcept {
        deleteThis();
        ptr     = rhs.ptr;
        rhs.ptr = nullptr;
        return *this;
    }

    ~UniquePtr() { deleteThis(); }

    T* get() const { return ptr; }

    T* release() {
        T* const result = ptr;
        ptr             = nullptr;
        return result;
    }

    T* operator->() const { return get(); }

    T& operator*() const { return *get(); }

    explicit operator bool() const { return get() != nullptr; }

    template <typename U>
    requires std::equality_comparable_with<T*, U*> bool
    operator==(UniquePtr<U> const& rhs) const {
        return get() == rhs.get();
    }

    bool operator==(std::nullptr_t) const { return get() == nullptr; }

    template <typename U>
    std::strong_ordering operator<=>(UniquePtr<U> const& rhs) const {
        return get() <=> rhs.get();
    }

    template <typename U>
    std::strong_ordering operator<=>(std::nullptr_t) const {
        return get() <=> nullptr;
    }

private:
    void deleteThis() {
        if (!ptr) {
            return;
        }
        /// Customization point
        ::scatha::internal::privateDelete(ptr);
    }

private:
    T* ptr;
};

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

template <typename T>
struct std::hash<scatha::UniquePtr<T>> {
    std::size_t operator()(scatha::UniquePtr<T> const& p) const {
        return std::hash<T const*>{}(p.get());
    }
};

#endif // SCATHA_COMMON_UNIQUEPTR_H_
