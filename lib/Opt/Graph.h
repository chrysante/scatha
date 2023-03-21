#ifndef SCATHA_OPT_GRAPH_H_
#define SCATHA_OPT_GRAPH_H_

#include <concepts>
#include <type_traits>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/vector.hpp>

namespace scatha::opt {

namespace internal {

template <typename Payload,
          bool IsTrivial =
              (sizeof(Payload) <= 16) && std::is_trivially_copyable_v<Payload>>
struct PayloadViewImpl /* IsTrivial = true*/ {
    using type = Payload;
};

template <typename Payload>
struct PayloadViewImpl<Payload, false> {
    using type = Payload const&;
};

template <typename Payload>
struct PayloadView: PayloadViewImpl<Payload> {};

template <>
struct PayloadView<void> {
    using type = void;
};

template <typename T>
struct PayloadWrapper {
    T value;
};

template <>
struct PayloadWrapper<void> {};

} // namespace internal

/// A generic graph or tree node. It is intended to be subclassed with CRTP.
///
/// \param Payload Can be anything including `void`
/// Some TMP trickery happens to ensure no unnecessary copies are made when
/// passing payloads from and to functions.
///
/// \param Derived The derived class, used by CRTP. Can also be void, then no
/// CRTP is happening.
///
/// \param IsTree Wether this node shall be a tree node.
/// If so, it implements a `.parent()` and a `.children()` method.
/// Otherwise it implements a `.predecessors()` and a `.successors()` method.
template <typename Payload, typename Derived = void, bool IsTree = false>
class GraphNode {
    /// Return and pass small trivial types by value, everything else by
    /// `const&`.
    using PayloadView = typename internal::PayloadView<Payload>::type;

    /// To add const-ness to the pointee if payload is a pointer.
    using PayloadViewConstPtr =
        std::conditional_t<std::is_pointer_v<PayloadView>,
                           std::remove_pointer_t<PayloadView> const*,
                           PayloadView>;

    static constexpr bool hasPayload = !std::is_same_v<Payload, void>;

    /// Either `GraphNode` or `Derived` is specified.
    using Self =
        std::conditional_t<std::is_same_v<Derived, void>, GraphNode, Derived>;

    /// For a tree node parent link is just a pointer, for a graph node it's a
    /// vector of pointers.
    using ParentLink =
        std::conditional_t<IsTree, Self*, utl::small_vector<Self*>>;

public:
    /// `PayloadHash` and `PayloadEqual` exist to put `GraphNode` in hashset
    /// where only the payload is used as the key.
    struct PayloadHash {
        using is_transparent = void;
        size_t operator()(GraphNode const& node) const {
            return (*this)(node.payload());
        }
        size_t operator()(PayloadViewConstPtr payload) const {
            return std::hash<std::decay_t<PayloadViewConstPtr>>{}(
                static_cast<PayloadViewConstPtr>(payload));
        }
    };

    /// See `PayloadHash`.
    struct PayloadEqual {
        using is_transparent = void;
        bool operator()(GraphNode const& a, GraphNode const& b) const {
            return a.payload() == b.payload();
        }
        bool operator()(GraphNode const& a, PayloadViewConstPtr b) const {
            return a.payload() == b;
        }
        bool operator()(PayloadViewConstPtr a, GraphNode const& b) const {
            return a == b.payload();
        }
    };

    GraphNode()
    requires std::is_default_constructible_v<internal::PayloadWrapper<Payload>>
        : _payload{} {}

    template <typename P = Payload>
    requires hasPayload
    explicit GraphNode(P const& payload): _payload{ payload } {}

    template <typename P = Payload>
    requires hasPayload
    explicit GraphNode(P&& payload): _payload{ std::move(payload) } {}

    PayloadView payload() const
    requires hasPayload
    {
        return _payload.value;
    }

    Self const& parent() const
    requires IsTree
    {
        return *_parentLink;
    }

    auto children() const
    requires IsTree
    {
        return outgoingImpl();
    }

    void setParent(Self* parent)
    requires IsTree
    {
        _parentLink = parent;
    }

    void addChild(Self* child)
    requires IsTree
    {
        addEdgeImpl(_outgoingEdges, child);
    }

    auto predecessors() const
    requires(!IsTree)
    {
        return incomingImpl();
    }

    void addPredecessor(Self* pred)
    requires(!IsTree)
    {
        addEdgeImpl(_parentLink, pred);
    }

    /// Add \p succ as successor if it is not already a successor
    void addSuccessor(Self* succ)
    requires(!IsTree)
    {
        addEdgeImpl(_outgoingEdges, succ);
    }

    /// \returns a view over references to successors
    auto successors() const
    requires(!IsTree)
    {
        return outgoingImpl();
    }

private:
    auto outgoingImpl() const {
        return _outgoingEdges | ranges::views::transform(
                                    [](auto* p) -> Self const& { return *p; });
    }

    auto incomingImpl() const {
        return _parentLink | ranges::views::transform(
                                 [](auto* p) -> Self const& { return *p; });
    }

    static void addEdgeImpl(utl::small_vector<Self*>& list, Self* other) {
        if (ranges::find(list, other) != ranges::end(list)) {
            return;
        }
        list.push_back(other);
    }

private:
    [[no_unique_address]] internal::PayloadWrapper<Payload> _payload;
    ParentLink _parentLink{};
    utl::small_vector<Self*> _outgoingEdges;
};

template <typename Payload, typename Derived = void, bool IsTree = false>
using TreeNode = GraphNode<Payload, Derived, true>;

} // namespace scatha::opt

#endif // SCATHA_OPT_GRAPH_H_
