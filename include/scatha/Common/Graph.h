#ifndef SCATHA_COMMON_GRAPH_H_
#define SCATHA_COMMON_GRAPH_H_

#include <concepts>
#include <queue>
#include <span>
#include <tuple>
#include <type_traits>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

namespace scatha {

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
class GraphNode;

namespace internal {

template <typename Payload,
          bool IsTrivial = (sizeof(Payload) <= 16) &&
                           std::is_trivially_copyable_v<Payload>>
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

template <typename Payload, typename Derived, GraphKind Kind>
class GraphNodeBase {
    template <typename, typename, GraphKind>
    friend class scatha::GraphNode;

    /// Return and pass small trivial types by value, everything else by
    /// `const&`.
    using PayloadView = typename internal::PayloadView<Payload>::type;

    /// To add const-ness to the pointee if payload is a pointer.
    using PayloadViewConstPtr =
        typename std::conditional_t<std::is_pointer_v<PayloadView>,
                                    std::remove_pointer<PayloadView>,
                                    std::type_identity<void>>::type const*;

    static constexpr bool HasPayload = !std::is_same_v<Payload, void>;

    /// Either `GraphNode` or `Derived` is specified.
    using Self = std::conditional_t<std::is_same_v<Derived, void>,
                                    GraphNode<Payload, Derived, Kind>,
                                    Derived>;

public:
    /// `PayloadHash` and `PayloadEqual` exist to put `GraphNode` in hashsets
    /// where only the payload is used as the key.
    template <typename Ptr = PayloadViewConstPtr>
        requires(!std::is_same_v<Ptr, void const*>)
    struct PayloadHash {
        using is_transparent = void;
        size_t operator()(Self const& node) const {
            return (*this)(node.payload());
        }
        size_t operator()(Ptr payload) const {
            return std::hash<std::decay_t<Ptr>>{}(static_cast<Ptr>(payload));
        }
    };

    /// See `PayloadHash`.
    template <typename Ptr = PayloadViewConstPtr>
        requires(!std::is_same_v<Ptr, void const*>)
    struct PayloadEqual {
        using is_transparent = void;
        bool operator()(Self const& a, Self const& b) const {
            return a.payload() == b.payload();
        }
        bool operator()(Self const& a, Ptr b) const { return a.payload() == b; }
        bool operator()(Ptr a, Self const& b) const { return a == b.payload(); }
    };

    GraphNodeBase()
        requires std::is_default_constructible_v<
            internal::PayloadWrapper<Payload>>
        : _payload{} {}

    template <typename P = Payload>
        requires HasPayload
    explicit GraphNodeBase(P&& payload): _payload{ std::forward<P>(payload) } {}

    PayloadView payload() const
        requires HasPayload
    {
        return _payload.value;
    }

    template <typename P = Payload>
        requires HasPayload
    void setPayload(P&& payload) {
        _payload.value = std::forward<P>(payload);
    }

private:
    static void addEdgeImpl(utl::small_vector<Self*>& list, Self* other) {
        if (ranges::find(list, other) != list.end()) {
            return;
        }
        list.push_back(other);
    }

    static void removeEdgeImpl(utl::small_vector<Self*>& list,
                               Self const* elem) {
        auto itr = ranges::find(list, elem);
        SC_ASSERT(itr != list.end(), "No edge present");
        list.erase(itr);
    }

    enum TraversalOrder { Preorder, Postorder };

    template <TraversalOrder Order, auto Successors, typename SELF, typename F>
    static void dfsImpl(SELF* self, F&& f) {
        if constexpr (Kind != GraphKind::Tree) {
            utl::hashset<SELF*> visited;
            dfsSearch<Order, Successors, SELF>(&visited, self, f);
        }
        else {
            dfsSearch<Order, Successors, SELF>(nullptr, self, f);
        }
    }

    template <TraversalOrder Order, auto Successors, typename SELF, typename F>
    static void dfsSearch(utl::hashset<SELF*>* visited, SELF* self, F&& f) {
        if (Kind != GraphKind::Tree && !visited->insert(self).second) {
            return;
        }
        if constexpr (Order == Preorder) {
            std::invoke(f, self);
        }
        for (auto* succ: std::invoke(Successors, self)) {
            dfsSearch<Order, Successors, SELF>(visited, succ, f);
        }
        if constexpr (Order == Postorder) {
            std::invoke(f, self);
        }
    }

    template <typename F, typename... Args>
    static auto invokeAsBool(F&& f, Args&&... args) {
        static constexpr bool ReturnsBool = requires {
            static_cast<bool>(
                std::invoke(std::forward<F>(f), std::forward<Args>(args)...));
        };
        if constexpr (ReturnsBool) {
            return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
        }
        else {
            std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
            return false;
        }
    }

    template <auto Successors, typename SELF, typename F>
    static auto bfsImpl(SELF* root, F&& f) {
        utl::hashset<SELF*> visited;
        std::queue<SELF*> queue;
        queue.push(root);
        if constexpr (Kind != GraphKind::Tree) {
            visited.insert(root);
        }
        while (!queue.empty()) {
            auto* node = queue.front();
            queue.pop();
            if (auto result = invokeAsBool(f, node)) {
                return result;
            }
            for (auto* succ: std::invoke(Successors, node)) {
                bool vis = false;
                if constexpr (Kind != GraphKind::Tree) {
                    vis = !visited.insert(succ).second;
                }
                if (!vis) {
                    queue.push(succ);
                }
            }
        }
        using InvokeResult = std::invoke_result_t<F&&, SELF*>;
        if constexpr (!std::is_same_v<void, std::remove_cv_t<InvokeResult>>) {
            return InvokeResult{};
        }
        else {
            return false;
        }
    }

    [[no_unique_address]] PayloadWrapper<Payload> _payload;
};

} // namespace internal

template <typename Payload, typename Derived>
class GraphNode<Payload, Derived, GraphKind::Undirected>:
    public internal::GraphNodeBase<Payload, Derived, GraphKind::Undirected> {
    using Base =
        internal::GraphNodeBase<Payload, Derived, GraphKind::Undirected>;
    using typename Base::Self;

public:
    using Base::Base;

    /// \returns a view over references to neighbours.
    std::span<Self* const> neighbours() { return edges; }

    /// \overload
    std::span<Self const* const> neighbours() const { return edges; }

    /// Add \p neigh as predecessor if it is not already a predecessor.
    void addNeighbour(Self* neigh) { Base::addEdgeImpl(edges, neigh); }

    /// \Returns Number of egdes.
    size_t degree() const { return edges.size(); }

private:
    utl::small_vector<Self*> edges;
};

template <typename Payload, typename Derived>
class GraphNode<Payload, Derived, GraphKind::Directed>:
    public internal::GraphNodeBase<Payload, Derived, GraphKind::Directed> {
    using Base = internal::GraphNodeBase<Payload, Derived, GraphKind::Directed>;
    using typename Base::Self;

    using TO = typename Base::TraversalOrder;

public:
    using Base::Base;

    /// \returns a view over pointers to predecessors.
    std::span<Self* const> predecessors() { return incoming; }

    /// \overload
    std::span<Self const* const> predecessors() const { return incoming; }

    /// \returns a view over pointers to successors.
    std::span<Self* const> successors() { return outgoing; }

    /// \overload
    std::span<Self const* const> successors() const { return outgoing; }

    /// \Returns Number of incoming egdes.
    size_t indegree() const { return incoming.size(); }

    /// \Returns Number of outgoing egdes.
    size_t outdegree() const { return outgoing.size(); }

    /// Add \p pred as predecessor if it is not already a predecessor.
    /// \Note this node needs to be set as a successor of \p pred manually
    void addPredecessor(Self* pred) { Base::addEdgeImpl(incoming, pred); }

    /// Add \p succ as successor if it is not already a successor.
    /// \Note this node needs to be set as a successor of \p pred manually
    void addSuccessor(Self* succ) { Base::addEdgeImpl(outgoing, succ); }

    /// Remove \p pred as predecessor
    /// \pre \p pred must be a predecessor of this node
    void removePredecessor(Self const* pred) {
        Base::removeEdgeImpl(incoming, pred);
    }

    /// Remove \p succ as successor
    /// \pre \p succ must be a successor of this node
    void removeSuccessor(Self const* succ) {
        Base::removeEdgeImpl(outgoing, succ);
    }

    /// \returns `true` if \p pred is an immediate predessecor of this node
    bool isPredecessor(Self const* pred) const {
        return ranges::contains(incoming, pred);
    }

    /// \returns `true` if \p succ is an immediate successor of this node
    bool isSuccessor(Self const* succ) const {
        return ranges::contains(outgoing, succ);
    }

    /// Clears all edges of this node.
    /// \warning Only removes edges from this node.
    /// Other nodes that reference this node are not updated.
    void clearEdges() {
        clearSuccessors();
        clearPredecessors();
    }

    /// \warning See `clearEdges()`
    void clearSuccessors() { outgoing.clear(); }

    /// \warning See `clearEdges()`
    void clearPredecessors() { incoming.clear(); }

    /// Traverse the graph from this node in DFS order and invoke callable \p f
    /// on every node before visiting its successors.
    template <typename F>
    void preorderDFS(F&& f) {
        this->template dfsImpl<TO::Preorder,
                               &GraphNode::outgoing>(static_cast<Self*>(this),
                                                     f);
    }

    /// \overload
    template <typename F>
    void preorderDFS(F&& f) const {
        this->template dfsImpl<TO::Preorder, &GraphNode::outgoing>(
            static_cast<Self const*>(this),
            f);
    }

    /// Traverse the graph from this node in DFS order and invoke callable \p f
    /// on every node after visiting its successors.
    template <typename F>
    void postorderDFS(F&& f) {
        this->template dfsImpl<TO::Postorder,
                               &GraphNode::outgoing>(static_cast<Self*>(this),
                                                     f);
    }

    /// \overload
    template <typename F>
    void postorderDFS(F&& f) const {
        this->template dfsImpl<TO::Postorder, &GraphNode::outgoing>(
            static_cast<Self const*>(this),
            f);
    }

private:
    utl::small_vector<Self*> incoming;
    utl::small_vector<Self*> outgoing;
};

template <typename Payload, typename Derived>
class GraphNode<Payload, Derived, GraphKind::Tree>:
    public internal::GraphNodeBase<Payload, Derived, GraphKind::Tree> {
    using Base = internal::GraphNodeBase<Payload, Derived, GraphKind::Tree>;
    using typename Base::Self;

    using TO = typename Base::TraversalOrder;

public:
    using Base::Base;

    /// \Returns a pointer to the parent node
    Self* parent() { return _parent; }

    /// \overload
    Self const* parent() const { return _parent; }

    /// \Returns a view over the children of this node
    std::span<Self* const> children() { return _children; }

    /// \overload
    std::span<Self const* const> children() const { return _children; }

    /// Add \p child as a child of this node. The parent pointer of\p child will
    /// be set to this node
    void addChild(Self* child) {
        SC_ASSERT(child != this, "Would form an invalid tree");
        child->_parent = static_cast<Self*>(this);
        Base::addEdgeImpl(_children, child);
    }

    /// Traverse the tree from this node in DFS order and invoke callable \p f
    /// on every node before visiting its successors.
    template <typename F>
    void preorderDFS(F&& f) {
        this->template dfsImpl<TO::Preorder,
                               &GraphNode::_children>(static_cast<Self*>(this),
                                                      f);
    }

    /// \overload
    template <typename F>
    void preorderDFS(F&& f) const {
        this->template dfsImpl<TO::Preorder, &GraphNode::_children>(
            static_cast<Self const*>(this),
            f);
    }

    /// Traverse the tree from this node in DFS order and invoke callable \p f
    /// on every node after visiting its successors.
    template <typename F>
    void postorderDFS(F&& f) {
        this->template dfsImpl<TO::Postorder,
                               &GraphNode::_children>(static_cast<Self*>(this),
                                                      f);
    }

    /// \overload
    template <typename F>
    void postorderDFS(F&& f) const {
        this->template dfsImpl<TO::Postorder, &GraphNode::_children>(
            static_cast<Self const*>(this),
            f);
    }

    /// Traverse the tree from this node in BFS order and invoke callable \p f
    /// on every node.
    template <typename F>
    auto BFS(F&& f) {
        return this->template bfsImpl<&GraphNode::_children>(static_cast<Self*>(
                                                                 this),
                                                             f);
    }

    /// \overload
    template <typename F>
    auto BFS(F&& f) const {
        return this
            ->template bfsImpl<&GraphNode::_children>(static_cast<Self const*>(
                                                          this),
                                                      f);
    }

private:
    Self* _parent = nullptr;
    utl::small_vector<Self*> _children;
};

template <typename Payload, typename Derived>
using TreeNode = GraphNode<Payload, Derived, GraphKind::Tree>;

/// # Simple graph queries

///
template <typename Node>
bool isCriticalEdge(Node const* from, Node const* to) {
    return from->successors().size() > 1 && to->predecessors().size() > 1;
}

} // namespace scatha

#endif // SCATHA_COMMON_GRAPH_H_
