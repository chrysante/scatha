#ifndef SCATHA_OPT_ACCESSTREE_H_
#define SCATHA_OPT_ACCESSTREE_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/Fwd.h"
#include "IR/Type.h"

namespace scatha::opt {

/// Represents accesses to objects of structure type
/// \Note We don't inherit from the generic graph class because here the tree
/// nodes own their children
class AccessTree {
    static auto childrenImpl(auto* self) { return self->_children | ToAddress; }

public:
    explicit AccessTree(ir::Type const* type, AccessTree* parent = nullptr,
                        size_t index = 0, bool isArrayNode = false):
        _type(type),
        _parent(parent),
        _index(index),
        _isArrayNode(isArrayNode) {}

    /// The type this leaf represents
    ir::Type const* type() const { return _type; }

    /// Range of child pointers
    auto children() { return childrenImpl(this); }

    /// \overload
    auto children() const { return childrenImpl(this); }

    /// Pointer to the child at index \p index
    /// May be null if`addSingleChild()` has been called on other indices but
    /// not on this one.
    AccessTree* childAt(size_t index) { return _children[index].get(); }

    /// \overload
    AccessTree const* childAt(size_t index) const {
        return _children[index].get();
    }

    /// Number of children this node has. Either 0 or the number of members of
    /// the type, if type is a structure type
    size_t numChildren() const { return _children.size(); }

    /// If any children have been allocated.
    bool hasChildren() const { return !isLeaf(); }

    /// \Returns `true` if any child nodes have constant values
    bool hasConstantChildren() const;

    /// Whether this node is a leaf
    bool isLeaf() const { return _children.empty(); }

    /// \Returns the sibling at offset \p offset if it exists, otherwise
    /// `nullptr`
    AccessTree* sibling(ssize_t offset);

    ///
    AccessTree* leftSibling() { return sibling(-1); }

    ///
    AccessTree* rightSibling() { return sibling(1); }

    /// Pointer to parent node
    AccessTree* parent() { return _parent; }

    /// \overload
    AccessTree const* parent() const { return _parent; }

    ///
    ir::Value* value() const { return _value; }

    ///
    void setValue(ir::Value* value) { _value = value; }

    /// The index of this node in it's parent
    std::optional<size_t> index() const {
        return parent() ? std::optional(_index) : std::nullopt;
    }

    /// Makes this node an array node and adds a child at index \p index
    AccessTree* addArrayChild(size_t index);

    /// Create children for every member type of this node's type,
    /// if it is a structure or array type.
    /// Incompatible with `addSingleChild()`
    void fanOut();

    /// Set a single child at index \p index
    ///
    /// \pre Type of this node must be a structure or array type.
    ///
    /// \Returns Child node
    ///
    /// Incompatible with `fanOut()`
    AccessTree* addSingleChild(size_t index);

    ///
    bool isArrayNode() const { return _isArrayNode; }

    /// Invoke \p callback for every leaf of this tree
    void leafWalk(utl::function_view<void(AccessTree* node,
                                          std::span<size_t const> indices)>
                      callback) {
        utl::small_vector<size_t> indices;
        leafWalkImpl(callback, indices);
    }

    /// \overload
    void leafWalk(utl::function_view<void(AccessTree* node)> callback) {
        leafWalk([&callback](AccessTree* node, std::span<size_t const>) {
            callback(node);
        });
    }

    /// Invoke \p callback for every node of this tree in post-order
    void postOrderWalk(utl::function_view<void(AccessTree* node,
                                               std::span<size_t const> indices)>
                           callback) {
        utl::small_vector<size_t> indices;
        postOrderWalkImpl(callback, indices);
    }

    /// Deep copy this tree making copies of the payloads
    std::unique_ptr<AccessTree> clone();

    /// Print this tree to `std::cout`
    void print() const;

    /// \overload
    void print(std::ostream& str) const;

private:
    void leafWalkImpl(utl::function_view<void(AccessTree* node,
                                              std::span<size_t const> indices)>
                          callback,
                      utl::vector<size_t>& indices) {
        if (isLeaf()) {
            callback(this, indices);
            return;
        }
        indices.push_back(0);
        for (auto* child: children()) {
            if (child) {
                child->leafWalkImpl(callback, indices);
            }
            ++indices.back();
        }
        indices.pop_back();
    }

    void postOrderWalkImpl(
        utl::function_view<void(AccessTree* node,
                                std::span<size_t const> indices)>
            callback,
        utl::vector<size_t>& indices) {
        indices.push_back(0);
        for (auto* child: children()) {
            if (child) {
                child->postOrderWalkImpl(callback, indices);
            }
            ++indices.back();
        }
        indices.pop_back();
        callback(this, indices);
    }

    /// Defined in constructor
    ir::Type const* _type = nullptr;
    AccessTree* _parent = nullptr;
    size_t _index = 0;
    bool _isArrayNode = false;

    utl::small_vector<std::unique_ptr<AccessTree>> _children;
    ir::Value* _value = nullptr;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_ACCESSTREE_H_
