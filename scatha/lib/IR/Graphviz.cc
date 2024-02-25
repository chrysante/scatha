#include "IR/Graphviz.h"

#include <iostream>
#include <string>

#include <graphgen/graphgen.h>
#include <range/v3/algorithm.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Common/PrintUtil.h"
#include "Debug/DebugGraphviz.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Module.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;
using namespace graphgen;

static constexpr auto MonoFont = "SF Mono";

static Label makeLabel(Function const& function) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    printDecl(function, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeLabel(BasicBlock const& BB) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    sstr << tableBegin() << rowBegin();
    sstr << tfmt::format(tfmt::Italic, "%", BB.name()) << ":" << rowEnd();
    for (auto& inst: BB) {
        sstr << rowBegin() << inst << rowEnd();
    }
    sstr << tableEnd();
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Graph* makeFunction(Function const& function) {
    auto* subgraph = Graph::make(ID(&function))->label(makeLabel(function));
    for (auto& BB: function) {
        subgraph->add(Vertex::make(ID(&BB))->label(makeLabel(BB)));
        for (auto* succ: BB.successors()) {
            subgraph->add(Edge{ ID(&BB), ID(succ) });
        }
    }
    return subgraph;
}

void ir::generateGraphviz(Function const& function, std::ostream& ostream) {
    Graph G;
    G.font(MonoFont);
    G.add(makeFunction(function));
    generate(G, ostream);
}

void ir::generateGraphviz(Module const& mod, std::ostream& ostream) {
    Graph G;
    G.font(MonoFont);
    for (auto& function: mod) {
        G.add(makeFunction(function));
    }
    generate(G, ostream);
}

static void generateTmpImpl(auto& obj) {
    try {
        auto [path, file] = debug::newDebugFile("cfg");
        generateGraphviz(obj, file);
        file.close();
        debug::createGraphAndOpen(path);
    }
    catch (std::exception const& e) {
        std::cout << e.what() << std::endl;
    }
}

void ir::generateGraphvizTmp(Function const& function) {
    generateTmpImpl(function);
}

void ir::generateGraphvizTmp(Module const& mod) { generateTmpImpl(mod); }
