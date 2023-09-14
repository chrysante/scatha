#ifndef SCATHA_OPT_MEMBERTREE_H_
#define SCATHA_OPT_MEMBERTREE_H_

#include <iosfwd>
#include <memory>

#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "IR/Fwd.h"

namespace scatha::opt {

///
///
///
class MemberTree {
    struct Payload {
        size_t index;
        ir::Type const* type;
        size_t begin, end;
    };

public:
    ///
    class Node: public TreeNode<Payload, Node> {
    public:
        Node(Payload payload) { setPayload(payload); }

        /// Index of this member in the parent type
        size_t index() const { return payload().index; }

        /// Type of this member
        ir::Type const* type() const { return payload().type; }

        /// Byte offset from the beginning of the root type to this member
        size_t begin() const { return payload().begin; }

        /// Byte offset from the beginning of the root type to the end of this
        /// member
        size_t end() const { return payload().end; }
    };

    ///
    static MemberTree compute(ir::Type const* type);

    ///
    Node const* root() const { return _root; }

private:
    Node* computeDFS(ir::Type const* type, size_t index, size_t offset);

    Node* _root;
    utl::small_vector<std::unique_ptr<Node>> nodes;
};

///
void print(MemberTree const& tree);

///
void print(MemberTree const& tree, std::ostream& ostream);

} // namespace scatha::opt

#endif // SCATHA_OPT_MEMBERTREE_H_
