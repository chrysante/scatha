#include "DotEmitter.h"

#include <fstream>
#include <sstream>

#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;

namespace {

struct Ctx {
    explicit Ctx(Module const& mod): mod(mod) {}

    void declare(Function const&);
    void declare(BasicBlock const&);

    void connect(Function const&);
    void connect(BasicBlock const&);

    void beginModule();
    void endModule();

    void beginFunction(ir::Function const& function);
    void endFunction();

    std::string dotName(Value const& value) const;

    std::string takeResult() { return std::move(str).str(); }

    Module const& mod;
    ir::Function const* currentFunction = nullptr;
    std::stringstream str;
    std::string font = "SF Mono";
};

} // namespace

std::string playground::emitDot(scatha::ir::Module const& mod) {
    Ctx ctx(mod);
    ctx.beginModule();
    for (auto& function: mod.functions()) {
        ctx.beginFunction(function);
        ctx.declare(function);
        ctx.connect(function);
        ctx.endFunction();
    }
    ctx.endModule();
    return ctx.takeResult();
}

void playground::emitDot(scatha::ir::Module const& mod, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << emitDot(mod);
}

void Ctx::declare(Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        declare(bb);
    }
}

inline constexpr utl::streammanip tableBegin = [](std::ostream& str, int border = 0, int cellborder = 0, int cellspacing = 0) {
    str << "<table border=\"" << border << "\" cellborder=\"" << cellborder << "\" cellspacing=\"" << cellspacing << "\">";
};

inline constexpr utl::streammanip tableEnd = [](std::ostream& str) {
    str << "</table>";
};

inline constexpr utl::streammanip fontBegin = [](std::ostream& str, std::string_view fontname) {
    str << "<font face=\"" << fontname << "\">";
};

inline constexpr utl::streammanip fontEnd = [](std::ostream& str) {
    str << "</font>";
};

inline constexpr utl::streammanip rowBegin = [](std::ostream& str) {
    str <<  "<tr><td align=\"left\">";
};

inline constexpr utl::streammanip rowEnd = [](std::ostream& str) {
    str <<  "</td></tr>";
};

void Ctx::declare(BasicBlock const& bb) {
    str << dotName(bb) << " [ label = <\n";
    auto prolog = [&]{ str << "    " << rowBegin << fontBegin(font) << "\n"; };
    auto epilog = [&]{ str << "    " << fontEnd << rowEnd << "\n"; };
    str << "  " << tableBegin << "\n";
    prolog();
    str << "%" << bb.name() << ":\n";
    epilog();
    for (auto& inst: bb.instructions) {
        prolog();
        str << inst << "\n";
        epilog();
    }
    str << "    " << tableEnd << "\n";
    str << ">]\n";
}

void Ctx::connect(Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        connect(bb);
    }
}

void Ctx::connect(BasicBlock const& bb) {
    for (auto& inst: bb.instructions) {
        if (!isa<TerminatorInst>(inst)) { continue; }
        // clang-format off
        visit(cast<TerminatorInst const&>(inst), utl::overload{
            [&](ir::TerminatorInst const& inst) {},
            [&](ir::Goto const& g) {
                str << dotName(bb) << " -> " << dotName(*g.target()) << "\n";
            },
            [&](ir::Branch const& b) {
                str << dotName(bb) << " -> " << dotName(*b.thenTarget()) << "\n";
                str << dotName(bb) << " -> " << dotName(*b.elseTarget()) << "\n";
            },
        });
        // clang-format on
     }
}

void Ctx::beginModule() {
    str << "digraph {\n";
    str << "  fontname = \"" << font << "\"\n";
    str << "  node [ shape = box ]\n";
}

void Ctx::endModule() {
    str << "} // digraph\n";
}

void Ctx::beginFunction(ir::Function const& function) {
    currentFunction = &function;
    str << "subgraph cluster_" << function.name() << " {\n";
    str << "  fontname = \"" << font << "\"\n";
    str << "  " << "label = \"@" << function.name() << "\"";
}

void Ctx::endFunction() {
    currentFunction = nullptr;
    str << "} // subgraph\n";
}

std::string Ctx::dotName(Value const& value) const {
    auto result = utl::strcat("_", currentFunction->name(), "_", value.name());
    std::replace(result.begin(), result.end(), '-', '_');
    return result;
}
