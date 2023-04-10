// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_COMMON_LIST_H_
#define SCATHA_COMMON_LIST_H_

#include <memory>

#include <utl/ilist.hpp>

#include <scatha/Common/UniquePtr.h> /// For privateDestroy()

namespace scatha {

template <typename T, bool AllowSetSiblings = false>
using ListNode = utl::ilist_node<T, AllowSetSiblings>;

/// Used to inherit from a class `Original` that already is a base class of
/// `ListNode<...>` Then, when `Derived` inherits from
/// `ListNodeOverride<Derived, Original>` _instead_ of inheriting from
/// `Original`, it exposes `prev()` and `next()` pointers typed as `Derived`.
template <typename Derived, typename Original>
class ListNodeOverride: public Original {
public:
    using Original::Original;

    Derived* prev() { return static_cast<Derived*>(asOriginal(this)->prev()); }

    Derived const* prev() const {
        return static_cast<Derived const*>(asOriginal(this)->prev());
    }

    Derived* next() { return static_cast<Derived*>(asOriginal(this)->next()); }

    Derived const* next() const {
        return static_cast<Derived const*>(asOriginal(this)->next());
    }

private:
    template <typename T>
    static utl::copy_cv_t<T, Original>* asOriginal(T* This) {
        return static_cast<utl::copy_cv_t<T, Derived>*>(This);
    }
};

template <typename Parent>
struct ParentedNode {
public:
    ParentedNode() = default;

    ParentedNode(Parent* p): p(p) {}

    Parent* parent() { return p; }

    Parent const* parent() const { return p; }

    void set_parent(Parent* parent) { p = parent; }

private:
    Parent* p = nullptr;
};

template <typename T>
class DynAllocator: public std::allocator<T> {
public:
    void destroy(T* ptr) { scatha::internal::privateDestroy(ptr); }

    void deallocate(T* ptr, size_t count) { ::operator delete(ptr, count); }
};

template <typename T>
using List = utl::ilist<T, DynAllocator<T>>;

/// Base class of `BasicBlock` and `Function` implementing their common
/// container-like interface. `ValueType` is `Instruction` for `BasicBlock` and
/// `BasicBlock` for `Function`.
template <typename Derived, typename ValueType>
class CFGList {
public:
    using Iterator      = typename List<ValueType>::iterator;
    using ConstIterator = typename List<ValueType>::const_iterator;

    /// Callee takes ownership.
    void pushFront(ValueType* value) { insert(values.begin(), value); }

    /// \overload
    void pushFront(UniquePtr<ValueType> value) { pushFront(value.release()); }

    /// Callee takes ownership.
    void pushBack(ValueType* value) { insert(values.end(), value); }

    /// \overload
    void pushBack(UniquePtr<ValueType> value) { pushBack(value.release()); }

    /// Callee takes ownership.
    Iterator insert(ConstIterator before, ValueType* value) {
        asDerived().insertCallback(*value);
        return values.insert(before, value);
    }

    /// \overload
    ValueType* insert(ValueType const* before, ValueType* value) {
        asDerived().insertCallback(*value);
        return insert(ConstIterator(before), value).to_address();
    }

    /// Merge `*this` with \p *rhs
    /// Insert nodes of \p *rhs before \p pos
    void splice(ConstIterator pos, Derived* rhs) {
        splice(pos, rhs->begin(), rhs->end());
    }

    /// Merge range `[begin, end)` into `*this`
    /// Insert nodes before \p pos
    void splice(ConstIterator pos, Iterator first, ConstIterator last) {
        for (auto itr = first; itr != last; ++itr) {
            SC_ASSERT(itr->parent() != this, "This is UB");
            asDerived().insertCallback(*itr);
        }
        values.splice(pos, first, last);
    }

    /// Clears the operands.
    Iterator erase(ConstIterator position) {
        SC_ASSERT(position->users().empty(),
                  "We should not erase this value when it's still in use");
        asDerived().eraseCallback(*position);
        return values.erase(position);
    }

    /// \overload
    Iterator erase(ValueType const* value) {
        return erase(ConstIterator(value));
    }

    /// \overload
    Iterator erase(ConstIterator first, ConstIterator last) {
        for (auto itr = first; itr != last; ++itr) {
            SC_ASSERT(itr->parent() == this, "This is UB");
            asDerived().eraseCallback(*itr);
        }
        return values.erase(first, last);
    }

    /// Extract a value. Caller takes ownership of the extracted value.
    UniquePtr<ValueType> extract(ConstIterator position) {
        return UniquePtr<ValueType>(values.extract(position));
    }

    /// \overload
    UniquePtr<ValueType> extract(ValueType const* value) {
        return extract(ConstIterator(value));
    }

    /// Destroy and deallocate all values in this list.
    void clear() {
        for (auto& value: values) {
            asDerived().eraseCallback(value);
        }
        values.clear();
    }

    Iterator begin() { return values.begin(); }
    ConstIterator begin() const { return values.begin(); }

    auto rbegin() { return values.rbegin(); }
    auto rbegin() const { return values.rbegin(); }

    Iterator end() { return values.end(); }
    ConstIterator end() const { return values.end(); }

    auto rend() { return values.rend(); }
    auto rend() const { return values.rend(); }

    bool empty() const { return values.empty(); }

    ValueType& front() { return values.front(); }
    ValueType const& front() const { return values.front(); }

    ValueType& back() { return values.back(); }
    ValueType const& back() const { return values.back(); }

private:
    Derived& asDerived() { return *static_cast<Derived*>(this); }

    /// Stubs
    void insertCallback(auto&&...) {}
    void eraseCallback(auto&&...) {}

private:
    List<ValueType> values;
};

} // namespace scatha

#endif // SCATHA_IR_LIST_H_
