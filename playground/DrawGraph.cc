#include "DrawGraph.h"

#include <fstream>
#include <ostream>
#include <sstream>
#include <string_view>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "Opt/SCCCallGraph.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

struct Ctx {
    explicit Ctx(
        Module const& mod,
        utl::function_view<void(std::stringstream&,
                                scatha::ir::Function const&)> functionCallback,
        utl::function_view<void(std::stringstream&,
                                scatha::ir::BasicBlock const&)>
            bbDeclareCallback,
        utl::function_view<void(std::stringstream&,
                                scatha::ir::BasicBlock const&)>
            bbConnectCallback):
        mod(mod),
        functionCallback(functionCallback),
        bbDeclareCallback(bbDeclareCallback),
        bbConnectCallback(bbConnectCallback) {}

    void run();

    void declare(Function const&);
    void declare(BasicBlock const&);

    void connect(Function const&);
    void connect(BasicBlock const&);

    void beginModule();
    void endModule();

    void beginFunction(ir::Function const& function);
    void endFunction();

    std::string takeResult() { return std::move(str).str(); }

    Module const& mod;
    utl::function_view<void(std::stringstream&, scatha::ir::Function const&)>
        functionCallback;
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)>
        bbDeclareCallback;
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)>
        bbConnectCallback;
    ir::Function const* currentFunction = nullptr;
    std::stringstream str;
};

static constexpr auto font     = "SF Pro";
static constexpr auto monoFont = "SF Mono";

} // namespace

static std::string dotName(Value const& value);

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
    scatha::ir::Module const& mod,
    utl::function_view<void(std::stringstream&, scatha::ir::Function const&)>
        functionCallback,
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)>
        bbDeclareCallback,
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)>
        bbConnectCallback) {
    Ctx ctx(mod, functionCallback, bbDeclareCallback, bbConnectCallback);
    ctx.run();
    return ctx.takeResult();
}

std::string playground::drawControlFlowGraph(scatha::ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str,
                               Function const& function) {};
    auto declareCallback  = [](std::stringstream& str, BasicBlock const& bb) {
        tfmt::setHTMLFormattable(str);
        str << dotName(bb) << " [ label = ";
        str << "<\n";
        auto prolog = [&] {
            str << "    " << rowBegin << fontBegin(monoFont) << "\n";
        };
        auto epilog = [&] { str << "    " << fontEnd << rowEnd << "\n"; };
        str << "  " << tableBegin << "\n";
        prolog();
        str << tfmt::format(tfmt::italic, "%", bb.name()) << ":\n";
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
    auto connectCallback = [](std::stringstream& str, BasicBlock const& bb) {
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
    scatha::ir::Module const& mod, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawControlFlowGraph(mod);
}

std::string playground::drawUseGraph(scatha::ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str,
                               Function const& function) {};
    auto declareCallback  = [](std::stringstream& str, BasicBlock const& bb) {
        str << "subgraph cluster_" << dotName(bb) << " {\n";
        str << "  fontname = \"" << monoFont << "\"\n";
        str << "  "
            << "label = \"%" << bb.name() << "\"";
        for (auto& inst: bb) {
            str << dotName(inst) << " [ label = \"" << inst << "\" ]\n";
        }
        str << "} // subgraph\n";
    };
    auto connectCallback = [](std::stringstream& str, BasicBlock const& bb) {
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

void playground::drawUseGraph(scatha::ir::Module const& mod,
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

void Ctx::declare(Function const& function) {
    for (auto& bb: function) {
        declare(bb);
    }
}

void Ctx::declare(BasicBlock const& bb) { bbDeclareCallback(str, bb); }

void Ctx::connect(Function const& function) {
    for (auto& bb: function) {
        connect(bb);
    }
}

void Ctx::connect(BasicBlock const& bb) { bbConnectCallback(str, bb); }

void Ctx::beginModule() {
    str << "digraph {\n";
    str << "  rankdir=TB;\n";
    str << "  compound=true;\n";
    str << "  node [ shape = box ]\n";
}

void Ctx::endModule() { str << "} // digraph\n"; }

void Ctx::beginFunction(ir::Function const& function) {
    currentFunction = &function;
    str << "subgraph cluster_" << function.name() << " {\n";
    str << "  fontname = \"" << monoFont << "\"\n";
    str << "  label = \"@" << function.name() << "\"\n";
    str << "  rank=same\n";
}

void Ctx::endFunction() {
    currentFunction = nullptr;
    str << "} // subgraph\n";
}

static std::string dotName(Value const& value) {
    Function const* currentFunction = [&] {
        if (auto* bb = dyncast<BasicBlock const*>(&value)) {
            return bb->parent();
        }
        return cast<Instruction const*>(&value)->parent()->parent();
    }();
    auto result = utl::strcat("_",
                              currentFunction->name(),
                              "_",
                              value.name(),
                              "_",
                              &value);
    std::replace_if(
        result.begin(),
        result.end(),
        [](char c) { return c == '.' || c == '-'; },
        '_');
    return result;
}

namespace {

struct CallGraphContext {
    CallGraphContext(SCCCallGraph const& callGraph): callGraph(callGraph) {}

    std::string run();

    void declare(SCCCallGraph::SCCNode const&);
    void declare(SCCCallGraph::FunctionNode const&);

    void connect(SCCCallGraph::SCCNode const&);
    void connect(SCCCallGraph::FunctionNode const&);

    size_t index(SCCCallGraph::SCCNode const& node) {
        auto itr = sccIndexMap.find(&node);
        if (itr == sccIndexMap.end()) {
            itr = sccIndexMap.insert({ &node, sccIndex++ }).first;
        }
        return itr->second;
    }

    SCCCallGraph const& callGraph;
    std::stringstream str;
    size_t sccIndex = 0;
    utl::hashmap<SCCCallGraph::SCCNode const*, size_t> sccIndexMap;
};

} // namespace

std::string playground::drawCallGraph(SCCCallGraph const& callGraph) {
    CallGraphContext ctx(callGraph);
    return ctx.run();
}

void playground::drawCallGraph(SCCCallGraph const& callGraph,
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

void CallGraphContext::declare(SCCCallGraph::SCCNode const& scc) {
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

void CallGraphContext::declare(SCCCallGraph::FunctionNode const& node) {
    str << "    " << node.function().name() << "\n";
}

void CallGraphContext::connect(SCCCallGraph::SCCNode const& scc) {
    for (auto* succ: scc.successors()) {
        str << "  " << scc.functions().front().name() << " -> "
            << succ->functions().front().name() << "[ltail=cluster_"
            << index(scc) << ", lhead=cluster_" << index(*succ) << "]"
            << "\n";
    }
    for (auto& func: scc.nodes()) {
        connect(func);
    }
}

void CallGraphContext::connect(SCCCallGraph::FunctionNode const& node) {
    for (auto* succ: node.successors()) {
        str << "  " << node.function().name() << " -> "
            << succ->function().name() << "\n";
        if (&node.scc() != &succ->scc()) {
            str << "[style=dashed, color=\"#00000080\", arrowhead=empty]\n";
        }
    }
}
