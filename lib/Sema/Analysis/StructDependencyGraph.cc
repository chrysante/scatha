#include "Sema/Analysis/StructDependencyGraph.h"

#include "Common/Base.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

size_t StructDependencyGraph::add(Node node) {
    SC_ASSERT(isa<StructureType>(node.entity) || isa<Variable>(node.entity),
              "Only structs and their data members shall be in this graph");
    size_t const index = _nodes.size();
    _indices.insert({ node.entity, index });
    _nodes.push_back(std::move(node));
    return index;
}
