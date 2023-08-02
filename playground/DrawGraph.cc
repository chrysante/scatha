#include "DrawGraph.h"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <string_view>

#include <range/v3/numeric.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "CodeGen/InterferenceGraph.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "MIR/Print.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;

namespace {

struct Ctx {
    explicit Ctx(
        ir::Module const& mod,
        utl::function_view<void(std::stringstream&, ir::Function const&)>
            functionCallback,
        utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
            bbDeclareCallback,
        utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
            bbConnectCallback):
        mod(mod),
        functionCallback(functionCallback),
        bbDeclareCallback(bbDeclareCallback),
        bbConnectCallback(bbConnectCallback) {}

    void run();

    void declare(ir::Function const&);
    void declare(ir::BasicBlock const&);

    void connect(ir::Function const&);
    void connect(ir::BasicBlock const&);

    void beginModule();
    void endModule();

    void beginFunction(ir::Function const& function);
    void endFunction();

    std::string takeResult() { return std::move(str).str(); }

    ir::Module const& mod;
    utl::function_view<void(std::stringstream&, ir::Function const&)>
        functionCallback;
    utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
        bbDeclareCallback;
    utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
        bbConnectCallback;
    ir::Function const* currentFunction = nullptr;
    std::stringstream str;
};

[[maybe_unused]] static constexpr auto font     = "SF Pro";
[[maybe_unused]] static constexpr auto monoFont = "SF Mono";

} // namespace

static std::string dotName(ir::Value const& value);

static constexpr utl::streammanip tableBegin = [](std::ostream& str,
                                                  int border      = 0,
                                                  int cellborder  = 0,
                                                  int cellspacing = 0) {
    str << "<table border=\"" << border << "\" cellborder=\"" << cellborder
        << "\" cellspacing=\"" << cellspacing << "\">";
};

static constexpr utl::streammanip tableEnd = [](std::ostream& str) {
    str << "</table>";
};

static constexpr utl::streammanip fontBegin =
    [](std::ostream& str, std::string_view fontname) {
    str << "<font face=\"" << fontname << "\">";
};

static constexpr utl::streammanip fontEnd = [](std::ostream& str) {
    str << "</font>";
};

static constexpr utl::streammanip rowBegin = [](std::ostream& str) {
    str << "<tr><td align=\"left\">";
};

static constexpr utl::streammanip rowEnd = [](std::ostream& str) {
    str << "</td></tr>";
};

std::string playground::drawGraphGeneric(
    ir::Module const& mod,
    utl::function_view<void(std::stringstream&, ir::Function const&)>
        functionCallback,
    utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
        bbDeclareCallback,
    utl::function_view<void(std::stringstream&, ir::BasicBlock const&)>
        bbConnectCallback) {
    Ctx ctx(mod, functionCallback, bbDeclareCallback, bbConnectCallback);
    ctx.run();
    return ctx.takeResult();
}

std::string playground::drawControlFlowGraph(scatha::ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str,
                               ir::Function const& function) {};
    auto declareCallback =
        [](std::stringstream& str, ir::BasicBlock const& bb) {
        tfmt::setHTMLFormattable(str);
        str << dotName(bb) << " [ label = ";
        str << "<\n";
        auto prolog = [&] {
            str << "    " << rowBegin << fontBegin(monoFont) << "\n";
        };
        auto epilog = [&] { str << "    " << fontEnd << rowEnd << "\n"; };
        str << "  " << tableBegin << "\n";
        prolog();
        str << tfmt::format(tfmt::Italic, "%", bb.name()) << ":\n";
        epilog();
        for (auto& inst: bb) {
            prolog();
            str << inst << "\n";
            epilog();
        }
        str << "    " << tableEnd << "\n";
        str << ">";
        str << "]\n";
    };
    auto connectCallback =
        [](std::stringstream& str, ir::BasicBlock const& bb) {
        for (auto succ: bb.successors()) {
            str << dotName(bb) << " -> " << dotName(*succ) << "\n";
        }
    };
    return drawGraphGeneric(mod,
                            functionCallback,
                            declareCallback,
                            connectCallback);
}

void playground::drawControlFlowGraph(
    ir::Module const& mod, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawControlFlowGraph(mod);
}

std::string playground::drawUseGraph(ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str,
                               ir::Function const& function) {};
    auto declareCallback =
        [](std::stringstream& str, ir::BasicBlock const& bb) {
        str << "subgraph cluster_" << dotName(bb) << " {\n";
        str << "  fontname = \"" << monoFont << "\"\n";
        str << "  "
            << "label = \"%" << bb.name() << "\"";
        for (auto& inst: bb) {
            str << dotName(inst) << " [ label = \"" << inst << "\" ]\n";
        }
        str << "} // subgraph\n";
    };
    auto connectCallback =
        [](std::stringstream& str, ir::BasicBlock const& bb) {
        for (auto& inst: bb) {
            for (auto* user: inst.users()) {
                str << dotName(inst) << " -> " << dotName(*user) << "\n";
            }
        }
    };
    return drawGraphGeneric(mod,
                            functionCallback,
                            declareCallback,
                            connectCallback);
}

void playground::drawUseGraph(ir::Module const& mod,
                              std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawUseGraph(mod);
}

void Ctx::run() {
    beginModule();
    for (auto& function: mod) {
        beginFunction(function);
        declare(function);
        functionCallback(str, function);
        connect(function);
        endFunction();
    }
    endModule();
}

void Ctx::declare(ir::Function const& function) {
    for (auto& bb: function) {
        declare(bb);
    }
}

void Ctx::declare(ir::BasicBlock const& bb) { bbDeclareCallback(str, bb); }

void Ctx::connect(ir::Function const& function) {
    for (auto& bb: function) {
        connect(bb);
    }
}

void Ctx::connect(ir::BasicBlock const& bb) { bbConnectCallback(str, bb); }

void Ctx::beginModule() {
    str << "digraph {\n";
    str << "  rankdir=TB;\n";
    str << "  compound=true;\n";
    str << "  node [ shape = box ]\n";
}

void Ctx::endModule() { str << "} // digraph\n"; }

void Ctx::beginFunction(ir::Function const& function) {
    currentFunction = &function;
    str << "subgraph cluster_" << dotName(function) << " {\n";
    str << "  fontname = \"" << monoFont << "\"\n";
    str << "  label = \"@" << function.name() << "\"\n";
    str << "  rank=same\n";
}

void Ctx::endFunction() {
    currentFunction = nullptr;
    str << "} // subgraph\n";
}

static std::string dotName(ir::Value const& value) {
    auto result = [&] {
        if (isa<ir::Function>(value)) {
            return utl::strcat("_", value.name(), "_", &value);
        }
        ir::Function const* currentFunction = [&] {
            if (auto* bb = dyncast<ir::BasicBlock const*>(&value)) {
                return bb->parent();
            }
            return cast<ir::Instruction const*>(&value)->parentFunction();
        }();
        return utl::strcat("_",
                           currentFunction->name(),
                           "_",
                           value.name(),
                           "_",
                           &value);
    }();
    std::replace_if(
        result.begin(),
        result.end(),
        [](char c) { return c == '.' || c == '-'; },
        '_');
    return result;
}

namespace {

struct CallGraphContext {
    CallGraphContext(opt::SCCCallGraph const& callGraph):
        callGraph(callGraph) {}

    std::string run();

    void declare(opt::SCCCallGraph::SCCNode const&);
    void declare(opt::SCCCallGraph::FunctionNode const&);

    void connect(opt::SCCCallGraph::SCCNode const&);
    void connect(opt::SCCCallGraph::FunctionNode const&);

    size_t index(opt::SCCCallGraph::SCCNode const& node) {
        auto itr = sccIndexMap.find(&node);
        if (itr == sccIndexMap.end()) {
            itr = sccIndexMap.insert({ &node, sccIndex++ }).first;
        }
        return itr->second;
    }

    opt::SCCCallGraph const& callGraph;
    std::stringstream str;
    size_t sccIndex = 0;
    utl::hashmap<opt::SCCCallGraph::SCCNode const*, size_t> sccIndexMap;
};

} // namespace

std::string playground::drawCallGraph(opt::SCCCallGraph const& callGraph) {
    CallGraphContext ctx(callGraph);
    return ctx.run();
}

void playground::drawCallGraph(opt::SCCCallGraph const& callGraph,
                               std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawCallGraph(callGraph);
}

std::string CallGraphContext::run() {
    str << "digraph {\n";
    str << "  rankdir=BT;\n";
    str << "  compound=true;\n";
    str << "  graph [ fontname=\"" << monoFont
        << "\", nodesep=0.5, ranksep=0.5 ];\n";
    str << "  node  [ fontname=\"" << monoFont << "\" ];\n";
    str << "  edge  [ fontname=\"" << monoFont << "\" ];\n";
    str << "  node  [ shape=ellipse ]\n";
    str << "\n  // We first declare all the nodes \n";
    for (auto& scc: callGraph.sccs()) {
        declare(scc);
    }
    str << "\n  // And then define the edges \n";
    for (auto& scc: callGraph.sccs()) {
        connect(scc);
    }
    str << "} // digraph\n";
    return std::move(str).str();
}

void CallGraphContext::declare(opt::SCCCallGraph::SCCNode const& scc) {
    str << "  subgraph cluster_" << index(scc) << " {\n";
    str << "    style=filled\n";
    auto const color = [&]() -> std::string {
        if (scc.predecessors().empty()) {
            if (scc.successors().empty()) {
                return "#00000011";
            }
            else {
                return "#0000ff11";
            }
        }
        else {
            if (scc.successors().empty()) {
                return "#ff000011";
            }
            else {
                return "#00000011";
            }
        }
    }();
    str << "    bgcolor=\"" << color << "\"\n";
    str << "    node [ shape=circle, style=filled, fillcolor=white ]\n";
    for (auto& function: scc.nodes()) {
        declare(function);
    }
    str << "  } // subgraph cluster_" << index(scc) << "\n\n";
}

void CallGraphContext::declare(opt::SCCCallGraph::FunctionNode const& node) {
    str << "    " << dotName(node.function()) << "\n";
}

void CallGraphContext::connect(opt::SCCCallGraph::SCCNode const& scc) {
    for (auto* succ: scc.successors()) {
        str << "  " << dotName(scc.functions().front()) << " -> "
            << dotName(succ->functions().front()) << "[ltail=cluster_"
            << index(scc) << ", lhead=cluster_" << index(*succ) << "]"
            << "\n";
    }
    for (auto& func: scc.nodes()) {
        connect(func);
    }
}

void CallGraphContext::connect(opt::SCCCallGraph::FunctionNode const& node) {
    for (auto* succ: node.successors()) {
        str << "  " << dotName(node.function()) << " -> "
            << dotName(succ->function()) << "\n";
        if (&node.scc() != &succ->scc()) {
            str << "[style=dashed, color=\"#00000080\", arrowhead=empty]\n";
        }
    }
}

namespace {

struct InterferenceGraphContext {
    using Node = cg::InterferenceGraph::Node;

    InterferenceGraphContext(mir::Function const& F):
        graph(cg::InterferenceGraph::compute(const_cast<mir::Function&>(F))) {
        graph.colorize();
        maxDegree =
            ranges::accumulate(graph, size_t(0), ranges::max, [](auto* node) {
                return node->degree();
            });
    }

    std::string draw() const {
        std::stringstream str;
        str << "graph {\n";
        str << "  rankdir=BT;\n";
        str << "  compound=true;\n";
        str << "  graph [ fontname=\"" << monoFont << "\" ];\n";
        str << "  node  [ \n";
        str << "      shape=circle, \n";
        str << "      fontname=\"" << monoFont << "\", \n";
        str << "      fontsize=\"20pt\", \n";
        str << "      fontcolor=\"white\", \n";
        str << "  ];\n";
        str << "\n";
        utl::hashset<std::pair<Node const*, Node const*>> drawnEdges;
        for (auto* n: graph) {
            str << toLabel(n) << "\n";
            for (auto* m: n->neighbours()) {
                if (drawnEdges.contains({ m, n })) {
                    continue;
                }
                drawnEdges.insert({ n, m });
                str << toName(n) << " -- " << toName(m) << "\n";
            }
        }
        str << "} // graph\n";
        return std::move(str).str();
    }

    std::string toName(Node const* node) const {
        return utl::strcat("node_", static_cast<void const*>(node));
    }

    std::string toColor(Node const* node) const {
        double const hue =
            static_cast<double>(node->color()) / graph.numColors();
        double const val = 0.8;
        std::stringstream str;
        str << std::setw(5) << std::fixed << hue << " 0.5 " << val << " 0.5";
        return std::move(str).str();
    }

    std::string toLabel(Node const* node) const {
        std::stringstream str;
        str << toName(node) << " [ \n";
        str << "    style=filled, \n";
        str << "    fillcolor=\"" << toColor(node) << "\", \n";
        str << "    height=1.5 \n";
        str << "    width=1.5 \n";
        str << "    label= \"%" << node->reg()->index() << " -> R"
            << node->color() << "\"";
        str << "]\n";
        return std::move(str).str();
    }

    cg::InterferenceGraph graph;
    size_t maxDegree = 0;
};

} // namespace

std::string playground::drawInterferenceGraph(mir::Function const& function) {
    InterferenceGraphContext ctx(function);
    return ctx.draw();
}

void playground::drawInterferenceGraph(
    mir::Function const& function, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawInterferenceGraph(function);
}
