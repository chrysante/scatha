#ifndef SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_
#define SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_

#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "AST/Base.h"
#include "Basic/Basic.h"
#include "Sema/Scope.h"
#include "Sema/SymbolID.h"

namespace scatha::sema {

struct DependencyGraphNode {
    SymbolID symbolID;
    SymbolCategory category;
    ast::AbstractSyntaxTree* astNode = nullptr;
    Scope* scope                     = nullptr;
    utl::small_vector<u16> dependencies;
};

class DependencyGraph {
public:
    size_t add(DependencyGraphNode node) {
        size_t const index = _nodes.size();
        _indices.insert({ node.symbolID, index });
        _nodes.push_back(std::move(node));
        return index;
    }

    auto begin() const { return _nodes.begin(); }
    auto begin() { return _nodes.begin(); }
    auto end() const { return _nodes.end(); }
    auto end() { return _nodes.end(); }

    DependencyGraphNode const& operator[](size_t index) const { return _nodes[index]; }
    DependencyGraphNode& operator[](size_t index) { return _nodes[index]; }

    size_t size() const { return _nodes.size(); }

    size_t index(SymbolID symbolID) const {
        auto const itr = _indices.find(symbolID);
        SC_EXPECT(itr != _indices.end(), "symbolID must be in this graph");
        return itr->second;
    }

private:
    utl::vector<DependencyGraphNode> _nodes;
    utl::hashmap<SymbolID, std::size_t> _indices;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_DEPENDENCYGRAPH_H_
