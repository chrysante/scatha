// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_LIST_H_
#define SCATHA_IR_LIST_H_

#include <memory>

#include <utl/ilist.hpp>

namespace scatha::ir {

template <typename T>
using Node = utl::ilist_node<T>;

template <typename T, typename Parent>
using NodeWithParent = utl::ilist_node_with_parent<T, Parent>;

template <typename T>
struct DynAllocator: std::allocator<T> {
    void destroy(T* ptr) {
        visit(*ptr, [](auto& obj) { std::destroy_at(&obj); });
    }
    
    void deallocate(T* ptr, size_t count) {
        operator delete(ptr, count);
    }
};

template <typename T>
using List = utl::ilist<T, DynAllocator<T>>;

} // namespace scatha::ir

#endif // SCATHA_IR_LIST_H_
