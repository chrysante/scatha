#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <utl/vector.hpp>

#include "Assembly.h"
#include "IRDump.h"
#include "IRSketch.h"
#include "SampleCompiler.h"
#include "DrawGraph.h"
#include "IR/Module.h"
#include "Opt/Mem2Reg.h"

enum class ProgramCase { SampleCompiler, IRDump, IRSketch, ASMTest, EmitCFG, EmitUseGraph };

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
    OptionParser parse = {
        { "sample-compiler", ProgramCase::SampleCompiler },
        { "ir-dump", ProgramCase::IRDump },
        { "ir-sketch", ProgramCase::IRSketch },
        { "test-asm", ProgramCase::ASMTest },
        { "emit-cfg", ProgramCase::EmitCFG },
        { "emit-use-graph", ProgramCase::EmitUseGraph },
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
    ProgramCase const theCase            = *parseResult;
    std::filesystem::path const filepath = std::filesystem::path(PROJECT_LOCATION) / "playground/Test.sc";
    using namespace playground;
    switch (theCase) {
    case ProgramCase::SampleCompiler: compile(filepath); break;
    case ProgramCase::IRDump: irDump(filepath); break;
    case ProgramCase::IRSketch: irSketch(); break;
    case ProgramCase::ASMTest: testAsmModule(); break;
    case ProgramCase::EmitCFG: {
        auto mod = makeIRModule(filepath);
        scatha::opt::mem2Reg(mod);
        drawControlFlowGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/cfg.gv");
        break;
    }
    case ProgramCase::EmitUseGraph: {
        auto mod = makeIRModule(filepath);
        drawUseGraph(mod, std::filesystem::path(PROJECT_LOCATION) / "graphviz/use-graph.gv");
        break;
    }
    default: break;
    }
}
