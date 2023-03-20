// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_IR_LIST_H_
#define SCATHA_IR_LIST_H_

#include <memory>

#include <utl/ilist.hpp>

#include <scatha/Common/UniquePtr.h> /// For privateDestroy()

namespace scatha::ir {

template <typename T>
using Node = utl::ilist_node<T>;

template <typename T, typename Parent>
using NodeWithParent = utl::ilist_node_with_parent<T, Parent>;

template <typename T>
class DynAllocator: public std::allocator<T> {
public:
    void destroy(T* ptr) {
        scatha::internal::privateDestroy(ptr);
    }

    void deallocate(T* ptr, size_t count) { ::operator delete(ptr, count); }
};

template <typename T>
using List = utl::ilist<T, DynAllocator<T>>;

} // namespace scatha::ir

#endif // SCATHA_IR_LIST_H_
