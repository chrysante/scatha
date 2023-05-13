#ifndef SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_
#define SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

struct DependencyGraphNode {
    Entity* entity                   = nullptr;
    ast::AbstractSyntaxTree* astNode = nullptr;
    utl::small_vector<u16> dependencies;
};

class DependencyGraph {
public:
    size_t add(DependencyGraphNode node) {
        size_t const index = _nodes.size();
        _indices.insert({ node.entity, index });
        _nodes.push_back(std::move(node));
        return index;
    }

    auto begin() const { return _nodes.begin(); }
    auto begin() { return _nodes.begin(); }
    auto end() const { return _nodes.end(); }
    auto end() { return _nodes.end(); }

    DependencyGraphNode const& operator[](size_t index) const {
        return _nodes[index];
    }
    DependencyGraphNode& operator[](size_t index) { return _nodes[index]; }

    size_t size() const { return _nodes.size(); }

    size_t index(Entity const* entity) const {
        auto const itr = _indices.find(entity);
        SC_EXPECT(itr != _indices.end(), "Entity must be in this graph");
        return itr->second;
    }

private:
    utl::vector<DependencyGraphNode> _nodes;
    utl::hashmap<Entity*, std::size_t> _indices;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_
