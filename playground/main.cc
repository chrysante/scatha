#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <utl/vector.hpp>

#include "APFloatTest.h"
#include "Assembly.h"
#include "DrawGraph.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IRDump.h"
#include "IRSketch.h"
#include "Opt/ConstantPropagation.h"
#include "Opt/Mem2Reg.h"
#include "OptTest.h"
#include "SampleCompiler.h"

enum class ProgramCase { SampleCompiler, IRDump, IRSketch, ASMTest, EmitCFG, EmitUseGraph, OptTest, APFloatTest };

struct Option {
    std::string id;
    ProgramCase target;
};

struct OptionParser {
    OptionParser(std::initializer_list<Option> options): OptionParser(utl::vector<Option>(options)) {}

    explicit OptionParser(utl::vector<Option> options): opts(std::move(options)) {
        for (auto& opt: opts) {
            opt.id.insert(0, "--");
        }
    }

    std::optional<ProgramCase> operator()(int argc, char const* const* argv) const {
        for (int i = 1; i < argc; ++i) {
            std::string_view const argument = argv[i];
            auto const itr = std::find_if(opts.begin(), opts.end(), [&](auto& opt) { return opt.id == argument; });
            if (itr != opts.end()) {
                return itr->target;
            }
        }
        return std::nullopt;
    }

    utl::vector<Option> opts;
};

int main(int argc, char const* const* argv) {
    OptionParser const parse = { { "sample-compiler", ProgramCase::SampleCompiler },
                                 { "ir-dump", ProgramCase::IRDump },
                                 { "ir-sketch", ProgramCase::IRSketch },
                                 { "test-asm", ProgramCase::ASMTest },
                                 { "emit-cfg", ProgramCase::EmitCFG },
                                 { "emit-use-graph", ProgramCase::EmitUseGraph },
                                 { "opt-test", ProgramCase::OptTest },
                                 { "apfloat-test", ProgramCase::APFloatTest } };
    auto const parseResult   = parse(argc, argv);
    if (!parseResult) {
        std::cerr << "Invalid usage: ";
        for (int i = 1; i < argc; ++i) {
            std::cout << argv[i] << " ";
        }
        std::cout << "\n";
        std::exit(EXIT_FAILURE);
    }
    ProgramCase const theCase            = *parseResult;
    std::filesystem::path const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    using namespace playground;
    switch (theCase) {
    case ProgramCase::SampleCompiler: compile(filepath); break;
    case ProgramCase::IRDump: irDumpFromFile(filepath); break;
    case ProgramCase::IRSketch: irSketch(); break;
    case ProgramCase::ASMTest: testAsmModule(); break;
    case ProgramCase::EmitCFG: {
        auto [ctx, mod] = makeIRModuleFromFile(filepath);
        drawControlFlowGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/cfg.gv");
        for (auto& function: mod.functions()) {
            scatha::opt::mem2Reg(ctx, function);
        }
        drawControlFlowGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/cfg-m2r.gv");
        for (auto& function: mod.functions()) {
            scatha::opt::propagateConstants(ctx, function);
        }
        drawControlFlowGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/cfg-scc.gv");
        break;
    }
    case ProgramCase::EmitUseGraph: {
        auto [ctx, mod] = makeIRModuleFromFile(filepath);
        drawUseGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/use-graph.gv");
        break;
    }
    case ProgramCase::OptTest: optTest(filepath); break;
    case ProgramCase::APFloatTest: apFloatTest(); break;
    default: break;
    }
}
