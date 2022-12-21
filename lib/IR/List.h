#ifndef SCATHA_IR_LIST_H_
#define SCATHA_IR_LIST_H_

#include <utl/ilist.hpp>

namespace scatha::ir {

template <typename T>
using Node = utl::ilist_node<T>;

template <typename T, typename Parent>
using NodeWithParent = utl::ilist_node_with_parent<T, Parent>;

template <typename T>
using List = utl::ilist<T>;

} // namespace scatha::ir

#endif // SCATHA_IR_LIST_H_
