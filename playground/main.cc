#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <utl/vector.hpp>

#include "CodeGen/Passes.h"
#include "DrawGraph.h"
#include "HostIntegration.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Print.h"
#include "IRDump.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"
#include "Opt/Optimizer.h"
#include "Opt/Passes.h"
#include "Opt/SCCCallGraph.h"
#include "SampleCompiler.h"
#include "Volatile.h"

enum class ProgramCase {
    SampleCompiler,
    IRDump,
    Volatile,
    EmitCFG,
    EmitCallGraph,
    EmitUseGraph,
    EmitInterferenceGraph,
    HostIntegration,
};

struct Option {
    std::string id;
    ProgramCase target;
};

struct OptionParser {
    OptionParser(std::initializer_list<Option> options):
        OptionParser(utl::vector<Option>(options)) {}

    explicit OptionParser(utl::vector<Option> options):
        opts(std::move(options)) {
        for (auto& opt: opts) {
            opt.id.insert(0, "--");
        }
    }

    std::optional<ProgramCase> operator()(int argc,
                                          char const* const* argv) const {
        for (int i = 1; i < argc; ++i) {
            std::string_view const argument = argv[i];
            auto const itr =
                std::find_if(opts.begin(), opts.end(), [&](auto& opt) {
                    return opt.id == argument;
                });
            if (itr != opts.end()) {
                return itr->target;
            }
        }
        return std::nullopt;
    }

    utl::vector<Option> opts;
};

int main(int argc, char const* const* argv) {
    OptionParser const parse = {
        { "sample-compiler", ProgramCase::SampleCompiler },
        { "ir-dump", ProgramCase::IRDump },
        { "volatile", ProgramCase::Volatile },
        { "emit-cfg", ProgramCase::EmitCFG },
        { "emit-callgraph", ProgramCase::EmitCallGraph },
        { "emit-use-graph", ProgramCase::EmitUseGraph },
        { "emit-interference-graph", ProgramCase::EmitInterferenceGraph },
        { "host-int", ProgramCase::HostIntegration },
    };
    auto const parseResult = parse(argc, argv);
    if (!parseResult) {
        std::cerr << "Invalid usage: ";
        for (int i = 1; i < argc; ++i) {
            std::cout << argv[i] << " ";
        }
        std::cout << "\n";
        std::exit(EXIT_FAILURE);
    }
    ProgramCase const theCase = *parseResult;
    std::filesystem::path const filepath =
        std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    using namespace playground;
    switch (theCase) {
    case ProgramCase::SampleCompiler:
        compile(filepath);
        break;
    case ProgramCase::IRDump:
        irDumpFromFile(filepath);
        break;
    case ProgramCase::Volatile:
        volatilePlayground(filepath);
        break;
    case ProgramCase::EmitCFG: {
        auto [ctx, mod] = makeIRModuleFromFile(filepath);
        drawControlFlowGraph(mod,
                             std::filesystem::path(PROJECT_LOCATION) /
                                 "graphviz/gen/cfg-none.gv");
        for (auto& function: mod) {
            scatha::opt::memToReg(ctx, function);
        }
        drawControlFlowGraph(mod,
                             std::filesystem::path(PROJECT_LOCATION) /
                                 "graphviz/gen/cfg-m2r.gv");
        for (auto& function: mod) {
            scatha::opt::propagateConstants(ctx, function);
        }
        drawControlFlowGraph(mod,
                             std::filesystem::path(PROJECT_LOCATION) /
                                 "graphviz/gen/cfg-scc.gv");
        for (auto& function: mod) {
            scatha::opt::dce(ctx, function);
        }
        drawControlFlowGraph(mod,
                             std::filesystem::path(PROJECT_LOCATION) /
                                 "graphviz/gen/cfg-dce.gv");

        scatha::opt::optimize(ctx, mod, 1);
        drawControlFlowGraph(mod,
                             std::filesystem::path(PROJECT_LOCATION) /
                                 "graphviz/gen/cfg-inl.gv");
        break;
    }
    case ProgramCase::EmitCallGraph: {
        auto [ctx, mod] = makeIRModuleFromFile(filepath);
        auto callGraph = scatha::opt::SCCCallGraph::compute(mod);
        drawCallGraph(callGraph,
                      std::filesystem::path(PROJECT_LOCATION) /
                          "graphviz/gen/callgraph.gv");
        break;
    }
    case ProgramCase::EmitUseGraph: {
        auto [ctx, mod] = makeIRModuleFromFile(filepath);
        drawUseGraph(mod,
                     std::filesystem::path(PROJECT_LOCATION) /
                         "graphviz/gen/use-graph.gv");
        break;
    }
    case ProgramCase::EmitInterferenceGraph: {
        auto [ctx, irMod] = makeIRModuleFromFile(filepath);
        scatha::opt::optimize(ctx, irMod, 1);
        auto mirMod = scatha::cg::lowerToMIR(irMod);
        auto& F = mirMod.front();
        scatha::cg::computeLiveSets(F);
        scatha::cg::deadCodeElim(F);
        scatha::cg::destroySSA(F);
        drawInterferenceGraph(F,
                              std::filesystem::path(PROJECT_LOCATION) /
                                  "graphviz/gen/interference-graph.gv");
        break;
    }
    case ProgramCase::HostIntegration:
        hostIntegration(std::filesystem::path(PROJECT_LOCATION) /
                        "playground/host-int.sc");
        break;
    default:
        break;
    }
}
