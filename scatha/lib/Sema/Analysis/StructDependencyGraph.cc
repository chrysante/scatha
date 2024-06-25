#include "Sema/Analysis/StructDependencyGraph.h"

#include <graphgen/graphgen.h>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Debug/DebugGraphviz.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace sema;

size_t StructDependencyGraph::add(Node node) {
    SC_ASSERT(
        isa<RecordType>(node.entity) || isa<BaseClassObject>(node.entity) ||
            isa<Variable>(node.entity),
        "Only records and their base classes and  data members shall be in this graph");
    size_t index = _nodes.size();
    _indices.insert({ node.entity, index });
    _nodes.push_back(std::move(node));
    return index;
}

using namespace graphgen;

Label makeLabel(ast::ASTNode const& node) {
    // clang-format off
    return SC_MATCH (node) {
        [](ast::TranslationUnit const&) {
            return Label("TU");
        },
        [](ast::Declaration const& decl) {
            return Label(utl::strcat(decl.nodeType(), ": ", decl.name()));
        },
        [](ast::Expression const& expr) {
            return Label(utl::strcat(expr.nodeType()));
        },
        [](ast::ASTNode const& node) {
            return Label(utl::strcat(node.nodeType()));
        },
    }; // clang-format on
}

void sema::generateDebugGraph(StructDependencyGraph const& SDG) {
    Graph G;
    for (auto& node: SDG) {
        auto* vertex = Vertex::make(ID(&node));
        vertex->label(makeLabel(*node.astNode));
        G.add(vertex);
        for (size_t index: node.dependencies) {
            G.add(Edge{ ID(&node), ID(ID(&SDG[index])) });
        }
    }
    auto [path, file] = debug::newDebugFile();
    generate(G, file);
    file.close();
    debug::createGraphAndOpen(path);
}
