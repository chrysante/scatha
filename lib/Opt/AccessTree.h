#ifndef SCATHA_OPT_ACCESSTREE_H_
#define SCATHA_OPT_ACCESSTREE_H_

#include <memory>
#include <optional>
#include <ostream>

#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "IR/Fwd.h"
#include "IR/Type.h"

namespace scatha::internal {

template <typename T>
struct VoidToEmpty {
    using type = T;
};

template <>
struct VoidToEmpty<void> {
    using type = struct {};
};

} // namespace scatha::internal

namespace scatha::opt {

/// Represents accesses to objects of structure type
template <typename Payload>
class AccessTree {
    static auto childrenImpl(auto* self) {
        return self->_children |
               ranges::views::transform([](auto& p) { return p.get(); });
    }

    using PayloadType = typename internal::VoidToEmpty<Payload>::type;

public:
    explicit AccessTree(ir::Type const* type): _type(type) {}

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

    /// Wether this node is a leaf
    bool isLeaf() const { return _children.empty(); }

    /// Pointer to parent node
    AccessTree* parent() { return _parent; }

    /// \overload
    AccessTree const* parent() const { return _parent; }

    /// Reference to payload
    auto const& payload() const { return _payload; }

    void setPayload(PayloadType payload) { _payload = std::move(payload); }

    /// The index of this node in it's parent
    std::optional<size_t> index() const {
        return parent() ? std::optional(_index) : std::nullopt;
    }

    /// Create children for every member type of this node's type,
    /// if it is a structure type.
    /// Incompatible with `addSingleChild()`
    void fanOut() {
        auto* sType = dyncast<ir::StructureType const*>(type());
        if (!sType) {
            return;
        }
        if (!_children.empty()) {
            SC_ASSERT(_children.size() == sType->members().size(), "");
            return;
        }
        for (auto [index, t]: sType->members() | ranges::views::enumerate) {
            _children.push_back(std::make_unique<AccessTree>(t));
            _children.back()->_index = index;
        }
    }

    /// Set a single child at index \p index
    ///
    /// \pre Type of this node must be a structure type.
    ///
    /// \Returns Child node
    ///
    /// Incompatible with `fanOut()`
    AccessTree* addSingleChild(size_t index) {
        auto* sType = dyncast<ir::StructureType const*>(_type);
        SC_ASSERT(sType && index < sType->members().size(), "Invalid index");
        if (_children.empty()) {
            _children.resize(sType->members().size());
        }
        auto& child = _children[index];
        if (!child) {
            child = std::make_unique<AccessTree>(sType->memberAt(index));
            child->_parent = this;
            child->_index  = index;
        }
        return child.get();
    }

    /// Invoke \p callback for every leaf of this tree
    void leafWalk(
        utl::function_view<void(AccessTree* node,
                                std::span<size_t const> indices)> callback) {
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
    void postOrderWalk(
        utl::function_view<void(AccessTree* node,
                                std::span<size_t const> indices)> callback) {
        utl::small_vector<size_t> indices;
        postOrderWalkImpl(callback, indices);
    }

    /// Deep copy this tree making copies of the payloads
    std::unique_ptr<AccessTree> clone() {
        auto result = std::make_unique<AccessTree>(type());
        result->_children.resize(_children.size());
        result->_index   = _index;
        result->_payload = _payload;
        for (auto&& [child, resChild]:
             ranges::views::zip(_children, result->_children))
        {
            if (child) {
                resChild          = child->clone();
                resChild->_parent = result.get();
            }
        }
        return result;
    }

    /// Print this tree
    template <typename PayloadTransform = std::identity>
    void print(std::ostream& str, auto pt = {}, int level = 0) const {
        for (int i = 0; i < level; ++i) {
            str << "    ";
        }
        if (index()) {
            str << *index() << ": ";
        }
        str << "[" << type()->name() << ", " << std::invoke(pt, payload())
            << "]\n";
        for (auto* c: children()) {
            if (c) {
                c->print(str, pt, level + 1);
            }
        }
    }

private:
    void leafWalkImpl(
        utl::function_view<void(AccessTree* node,
                                std::span<size_t const> indices)> callback,
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
                                std::span<size_t const> indices)> callback,
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

protected:
    PayloadType _payload{};

private:
    AccessTree* _parent = nullptr;
    utl::small_vector<std::unique_ptr<AccessTree>> _children;
    ir::Type const* _type = nullptr;
    size_t _index         = 0;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_ACCESSTREE_H_
