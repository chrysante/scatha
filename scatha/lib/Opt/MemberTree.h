#ifndef SCATHA_OPT_MEMBERTREE_H_
#define SCATHA_OPT_MEMBERTREE_H_

#include <iosfwd>
#include <memory>

#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "Common/Graph.h"
#include "IR/Fwd.h"

namespace scatha::opt {

/// Tree structure representing the nested members of struct and array types.
/// This class is used by SROA to analyze memory accesses to struct members
class MemberTree {
public:
    /// Represents a data member of a structure or an element in an arry
    class Node: public TreeNode<void, Node> {
    public:
        Node(size_t index, ir::Type const* type, size_t begin, size_t end):
            _index(index), _type(type), _begin(begin), _end(end) {}

        /// Index of this member in the parent type
        size_t index() const { return _index; }

        /// Type of this member
        ir::Type const* type() const { return _type; }

        /// Byte offset from the beginning of the root type to this member
        size_t begin() const { return _begin; }

        /// Byte offset from the beginning of the root type to the end of this
        /// member
        size_t end() const { return _end; }

    private:
        size_t _index;
        ir::Type const* _type;
        size_t _begin, _end;
    };

    /// Computes the member tree of \p type
    static MemberTree compute(ir::Type const* type);

    /// \Returns the root of the tree
    Node const* root() const { return _root; }

private:
    /// Implementation of `compute()`
    Node* computeDFS(ir::Type const* type, size_t index, size_t offset);

    Node* _root;
    MonotonicBufferAllocator allocator;
};

/// Prints the member tree \p tree to `std::cout`
void print(MemberTree const& tree);

/// Prints the member tree \p tree to \p ostream
void print(MemberTree const& tree, std::ostream& ostream);

} // namespace scatha::opt

#endif // SCATHA_OPT_MEMBERTREE_H_
