#ifndef SCATHA_OPT_ACCESSTREE_H_
#define SCATHA_OPT_ACCESSTREE_H_

#include <memory>
#include <optional>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include "IR/Common.h"
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

    ir::Type const* type() const { return _type; }

    auto children() { return childrenImpl(this); }

    auto children() const { return childrenImpl(this); }

    AccessTree* childAt(size_t index) { return _children[index].get(); }

    AccessTree const* childAt(size_t index) const {
        return _children[index].get();
    }

    size_t numChildren() const { return _children.size(); }

    bool hasChildren() const { return !_children.empty(); }

    AccessTree* parent() { return _parent; }

    AccessTree const* parent() const { return _parent; }

    auto const& payload() const { return _payload; }

    void setPayload(PayloadType payload) { _payload = std::move(payload); }

    /// The index of this node in it's parent
    std::optional<size_t> index() const {
        return parent() ? std::nullopt : std::optional(_index);
    }

    /// Create children for every member type of this node's type,
    /// if it is a structure type.
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
