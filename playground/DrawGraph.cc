#include "DrawGraph.h"

#include <sstream>
#include <fstream>
#include <ostream>
#include <string_view>

#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "IR/CFG.h"
#include "IR/Module.h"
#include "IR/Print.h"

using namespace scatha;
using namespace ir;

namespace {

struct Ctx {
    explicit Ctx(Module const& mod,
                 utl::function_view<void(std::stringstream&, scatha::ir::Function const&)> functionCallback,
                 utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbDeclareCallback,
                 utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbConnectCallback):
        mod(mod), functionCallback(functionCallback), bbDeclareCallback(bbDeclareCallback), bbConnectCallback(bbConnectCallback) {}

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
    utl::function_view<void(std::stringstream&, scatha::ir::Function const&)> functionCallback;
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbDeclareCallback;
    utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbConnectCallback;
    ir::Function const* currentFunction = nullptr;
    std::stringstream str;
};

static constexpr auto font = "SF Mono";

} // namespace

static std::string dotName(Value const& value);

static constexpr utl::streammanip tableBegin =
    [](std::ostream& str, int border = 0, int cellborder = 0, int cellspacing = 0) {
    str << "<table border=\"" << border << "\" cellborder=\"" << cellborder << "\" cellspacing=\"" << cellspacing
        << "\">";
};

static constexpr utl::streammanip tableEnd = [](std::ostream& str) { str << "</table>"; };

static constexpr utl::streammanip fontBegin = [](std::ostream& str, std::string_view fontname) {
    str << "<font face=\"" << fontname << "\">";
};

static constexpr utl::streammanip fontEnd = [](std::ostream& str) { str << "</font>"; };

static constexpr utl::streammanip rowBegin = [](std::ostream& str) { str << "<tr><td align=\"left\">"; };

static constexpr utl::streammanip rowEnd = [](std::ostream& str) { str << "</td></tr>"; };

std::string playground::drawGraphGeneric(scatha::ir::Module const& mod,
                                         utl::function_view<void(std::stringstream&, scatha::ir::Function const&)> functionCallback,
                                         utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbDeclareCallback,
                                         utl::function_view<void(std::stringstream&, scatha::ir::BasicBlock const&)> bbConnectCallback)
{
    Ctx ctx(mod, functionCallback, bbDeclareCallback, bbConnectCallback);
    ctx.run();
    return ctx.takeResult();
}

std::string playground::drawControlFlowGraph(scatha::ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str, Function const& function) {};
    auto declareCallback = [](std::stringstream& str, BasicBlock const& bb) {
        str << dotName(bb) << " [ label = ";
        str << "<\n";
        auto prolog = [&] { str << "    " << rowBegin << fontBegin(font) << "\n"; };
        auto epilog = [&] { str << "    " << fontEnd << rowEnd << "\n"; };
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
        str << ">";
        str << "]\n";
    };
    auto connectCallback = [](std::stringstream& str, BasicBlock const& bb) {
        for (auto succ: bb.successors) {
            str << dotName(bb) << " -> " << dotName(*succ) << "\n";
        }
    };
    return drawGraphGeneric(mod, functionCallback, declareCallback, connectCallback);
}

void playground::drawControlFlowGraph(scatha::ir::Module const& mod, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawControlFlowGraph(mod);
}

std::string playground::drawUseGraph(scatha::ir::Module const& mod) {
    auto functionCallback = [](std::stringstream& str, Function const& function) {};
    auto declareCallback = [](std::stringstream& str, BasicBlock const& bb) {
        str << "subgraph cluster_" << dotName(bb) << " {\n";
        str << "  fontname = \"" << font << "\"\n";
        str << "  "
            << "label = \"%" << bb.name() << "\"";
        for (auto& inst: bb.instructions) {
            str << dotName(inst) << " [ label = \"" << inst << "\" ]\n";
        }
        str << "} // subgraph\n";
    };
    auto connectCallback = [](std::stringstream& str, BasicBlock const& bb) {
        for (auto& inst: bb.instructions) {
            for (auto* user: inst.users()) {
                str << dotName(inst) << " -> " << dotName(*user) << "\n";
            }
        }
    };
    return drawGraphGeneric(mod, functionCallback, declareCallback, connectCallback);
}

void playground::drawUseGraph(scatha::ir::Module const& mod, std::filesystem::path const& outFilepath) {
    std::fstream file(outFilepath, std::ios::out | std::ios::trunc);
    file << drawUseGraph(mod);
}

void Ctx::run() {
    beginModule();
    for (auto& function: mod.functions()) {
        beginFunction(function);
        declare(function);
        functionCallback(str, function);
        connect(function);
        endFunction();
    }
    endModule();
}

void Ctx::declare(Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        declare(bb);
    }
}

void Ctx::declare(BasicBlock const& bb) {
    bbDeclareCallback(str, bb);
}

void Ctx::connect(Function const& function) {
    for (auto& bb: function.basicBlocks()) {
        connect(bb);
    }
}

void Ctx::connect(BasicBlock const& bb) {
    bbConnectCallback(str, bb);
}

void Ctx::beginModule() {
    str << "digraph {\n";
    str << "  rankdir=LR;\n";
    str << "  compound=true;\n";
    str << "  fontname = \"" << font << "\";\n";
    str << "  node [ shape = box ]\n";
}

void Ctx::endModule() {
    str << "} // digraph\n";
}

void Ctx::beginFunction(ir::Function const& function) {
    currentFunction = &function;
    str << "subgraph cluster_" << function.name() << " {\n";
    str << "  fontname = \"" << font << "\"\n";
    str << "  "
        << "label = \"@" << function.name() << "\"";
}

void Ctx::endFunction() {
    currentFunction = nullptr;
    str << "} // subgraph\n";
}

static std::string dotName(Value const& value) {
    Function const* currentFunction = [&]{
        if (auto* bb = dyncast<BasicBlock const*>(&value)) {
            return bb->parent();
        }
        return cast<Instruction const*>(&value)->parent()->parent();
    }();
    auto result = utl::strcat("_", currentFunction->name(), "_", value.name(), "_", &value);
    std::replace(result.begin(), result.end(), '-', '_');
    return result;
}
