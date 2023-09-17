#include "IR/Graphviz.h"

#include <string>

#include <graphgen/graphgen.h>
#include <range/v3/algorithm.hpp>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;
using namespace graphgen;

static constexpr auto monoFont = "SF Mono";

static constexpr utl::streammanip tableBegin = [](std::ostream& str,
                                                  int border = 0,
                                                  int cellborder = 0,
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

static Label makeLabel(Function const& function) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    printDecl(function, sstr);
    return Label(std::move(sstr).str(), LabelKind::HTML);
}

static Label makeLabel(BasicBlock const& BB) {
    std::stringstream sstr;
    tfmt::setHTMLFormattable(sstr);
    auto prolog = [&] {
        sstr << "    " << rowBegin << fontBegin(monoFont) << "\n";
    };
    auto epilog = [&] { sstr << "    " << fontEnd << rowEnd << "\n"; };
    sstr << tableBegin << "\n";
    prolog();
    sstr << tfmt::format(tfmt::Italic, "%", BB.name()) << ":\n";
    epilog();
    for (auto& inst: BB) {
        prolog();
        sstr << inst << "\n";
        epilog();
    }
    sstr << tableEnd << "\n";
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
    G.font(monoFont);
    G.add(makeFunction(function));
    generate(G, ostream);
}

void ir::generateGraphviz(Module const& mod, std::ostream& ostream) {
    Graph G;
    G.font(monoFont);
    for (auto& function: mod) {
        G.add(makeFunction(function));
    }
    generate(G, ostream);
}
