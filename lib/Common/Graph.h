#ifndef SCATHA_COMMON_GRAPH_H_
#define SCATHA_COMMON_GRAPH_H_

#include <concepts>
#include <tuple>
#include <type_traits>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha {

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

template <size_t Index, typename... T>
using TypeSwitch = std::tuple_element_t<Index, std::tuple<T...>>;

struct Empty {};

} // namespace internal

/// To be used as template parameter of `class GraphNode<>`
enum class GraphKind { Undirected, Directed, Tree };

/// A generic graph or tree node. It is intended to be subclassed with CRTP.
///
/// \param Payload Can be anything including `void`
/// Some TMP trickery happens to ensure no unnecessary copies are made when
/// passing payloads from and to functions.
///
/// \param Derived The derived class, used by CRTP. Can also be void, then no
/// CRTP is happening.
///
/// \param Kind Depending on the kind, this class implements different
/// interfaces:
/// - `UndirectedGraph`: Implements `.neighbours()` method.
/// - `DirectedGraph`: Implements `.predecessors()` and `.successors()` methods.
/// - `Tree`: Implements `.parent()` and `.children()` methods.
template <typename Payload, typename Derived, GraphKind Kind>
class GraphNode {
    /// Return and pass small trivial types by value, everything else by
    /// `const&`.
    using PayloadView = typename internal::PayloadView<Payload>::type;

    /// To add const-ness to the pointee if payload is a pointer.
    using PayloadViewConstPtr =
        std::conditional_t<std::is_pointer_v<PayloadView>,
                           std::remove_pointer_t<PayloadView> const*,
                           PayloadView>;

    static constexpr bool HasPayload = !std::is_same_v<Payload, void>;

    /// Either `GraphNode` or `Derived` is specified.
    using Self =
        std::conditional_t<std::is_same_v<Derived, void>, GraphNode, Derived>;

    /// For an undirected graph, parent links don't exist.
    /// For a digraph node parent links are vectors of pointers.
    /// For a tree, it's a single pointer.
    using ParentLink = internal::TypeSwitch<static_cast<size_t>(Kind),
                                            internal::Empty,
                                            utl::small_vector<Self*>,
                                            Self*>;

public:
    /// `PayloadHash` and `PayloadEqual` exist to put `GraphNode` in hashsets
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

    /// ## Common

    GraphNode()
        requires std::is_default_constructible_v<
            internal::PayloadWrapper<Payload>>
        : _payload{} {}

    template <typename P = Payload>
        requires HasPayload
    explicit GraphNode(P const& payload): _payload{ payload } {}

    template <typename P = Payload>
        requires HasPayload
    explicit GraphNode(P&& payload): _payload{ std::move(payload) } {}

    PayloadView payload() const
        requires HasPayload
    {
        return _payload.value;
    }

    ///
    /// # Undirected graph node members
    ///

    /// \returns a view over references to neighbours.
    auto neighbours() const
        requires(Kind == GraphKind::Undirected)
    {
        return outgoingImpl();
    }

    /// Add \p neigh as predecessor if it is not already a predecessor.
    void addNeighbour(Self* neigh)
        requires(Kind == GraphKind::Undirected)
    {
        addEdgeImpl(_outgoingEdges, neigh);
    }

    ///
    /// # Directed graph node members
    ///

    /// \returns a view over references to predecessors.
    auto predecessors() const
        requires(Kind == GraphKind::Directed)
    {
        return incomingImpl();
    }

    /// Add \p pred as predecessor if it is not already a predecessor.
    void addPredecessor(Self* pred)
        requires(Kind == GraphKind::Directed)
    {
        addEdgeImpl(_parentLink, pred);
    }

    /// \returns a view over references to successors.
    auto successors() const
        requires(Kind == GraphKind::Directed)
    {
        return outgoingImpl();
    }

    /// Add \p succ as successor if it is not already a successor.
    void addSuccessor(Self* succ)
        requires(Kind == GraphKind::Directed)
    {
        addEdgeImpl(_outgoingEdges, succ);
    }

    ///
    /// # Tree node members
    ///

    Self const* parent() const
        requires(Kind == GraphKind::Tree)
    {
        return _parentLink;
    }

    auto children() const
        requires(Kind == GraphKind::Tree)
    {
        return outgoingImpl();
    }

    void setParent(Self* parent)
        requires(Kind == GraphKind::Tree)
    {
        SC_ASSERT(parent != this, "Would form an invalid tree");
        _parentLink = parent;
    }

    void addChild(Self* child)
        requires(Kind == GraphKind::Tree)
    {
        SC_ASSERT(child != this, "Would form an invalid tree");
        addEdgeImpl(_outgoingEdges, child);
    }

private:
    std::span<Self const* const> outgoingImpl() const { return _outgoingEdges; }

    std::span<Self const* const> incomingImpl() const { return _parentLink; }

    static void addEdgeImpl(utl::small_vector<Self*>& list, Self* other) {
        if (ranges::find(list, other) != ranges::end(list)) {
            return;
        }
        list.push_back(other);
    }

private:
    [[no_unique_address]] internal::PayloadWrapper<Payload> _payload;
    [[no_unique_address]] ParentLink _parentLink{};
    utl::small_vector<Self*> _outgoingEdges;
};

template <typename Payload, typename Derived>
using TreeNode = GraphNode<Payload, Derived, GraphKind::Tree>;

} // namespace scatha

#endif // SCATHA_COMMON_GRAPH_H_
