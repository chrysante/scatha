#ifndef SCATHA_SEMA_ANALYSIS_STRUCTDEPENDENCYGRAPH_H_
#define SCATHA_SEMA_ANALYSIS_STRUCTDEPENDENCYGRAPH_H_

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Graph of all struct types and their data members in the program.
/// Edges mean that the predecessor depends on the successor. For example in the
/// following scenario `X` depends on `y` and `y` depends on `Y`, because to
/// instantiate `X` we need to know the type of `y` (for the size), which means
/// there must be an entry for `Y` in the symbol table. We instantiate struct
/// types and their data members in topsort order if this graph, i.e. starting
/// from the sinks.
/// ```
/// struct X {
///    var y: Y;
/// }
/// ```
/// This graph will be topologically sorted in `instantiateEntities()`
class StructDependencyGraph {
public:
    /// Reference to a struct type or a data member of a struct type
    /// Holds a pointer to both the `sema::Entity` and the defining AST node
    struct Node {
        Entity* entity = nullptr;
        ast::ASTNode* astNode = nullptr;
        utl::small_vector<u16> dependencies = {};
    };

    /// Add a node to the graph
    size_t add(Node node);

    /// \Returns the node at index \p index
    Node const& operator[](size_t index) const { return _nodes[index]; }

    ///  \overload
    Node& operator[](size_t index) { return _nodes[index]; }

    /// \Returns  the number of nodes in the graph
    size_t size() const { return _nodes.size(); }

    /// \Returns the index of \p entity in the graph
    size_t index(Entity const* entity) const {
        auto const itr = _indices.find(entity);
        SC_ASSERT(itr != _indices.end(), "Entity must be in this graph");
        return itr->second;
    }

    auto begin() const { return _nodes.begin(); }
    auto begin() { return _nodes.begin(); }
    auto end() const { return _nodes.end(); }
    auto end() { return _nodes.end(); }

private:
    utl::vector<Node> _nodes;
    utl::hashmap<Entity*, std::size_t> _indices;
};

SCTEST_API void generateDebugGraph(StructDependencyGraph const&);

using SDGNode = StructDependencyGraph::Node;

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_STRUCTDEPENDENCYGRAPH_H_
