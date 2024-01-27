#include "Compiler.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <range/v3/view.hpp>
#include <scatha/Assembly/Assembler.h>
#include <scatha/Assembly/AssemblyStream.h>
#include <scatha/CodeGen/CodeGen.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Context.h>
#include <scatha/IR/Module.h>
#include <scatha/IR/Print.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Invocation/ExecutableWriter.h>
#include <scatha/Sema/Entity.h>
#include <scatha/Sema/Serialize.h>
#include <utl/format_time.hpp>
#include <utl/strcat.hpp>

#include "Util.h"

using namespace scatha;
using namespace ranges::views;

namespace {

struct Timer {
    using Clock = std::chrono::high_resolution_clock;

    Timer() { reset(); }

    Clock::duration elapsed() const { return Clock::now() - start; }

    void reset() { start = Clock::now(); }

    Clock::time_point start;
};

} // namespace

int scatha::compilerMain(CompilerOptions options) {
    CompilerInvocation invocation;
    invocation.setInputs(options.files | transform(&SourceFile::load) |
                         ranges::to<std::vector>);
    invocation.setLibSearchPaths(options.libSearchPaths);
    Timer timer;
    auto makePrintCallback = [&](std::string section) {
        return [&, section](auto&&...) {
            if (options.time) {
                std::cout << section << ": "
                          << utl::format_duration(timer.elapsed()) << std::endl;
                timer.reset();
            }
        };
    };
    invocation.setCallbacks({
        .frontendCallback = makePrintCallback("Frontend"),
        .irgenCallback = makePrintCallback("IR generation"),
        .optCallback = makePrintCallback("Optimizer"),
        .codegenCallback = makePrintCallback("Codegen"),
        .asmCallback = makePrintCallback("Assembler"),
        .linkerCallback = makePrintCallback("Linker"),
    });
    invocation.setTargetType(options.targetType);
    invocation.setFrontend(deduceFrontend(options.files));
    if (options.outputFile.empty()) {
        options.outputFile = "out";
    }
    invocation.setOutputFile(options.outputFile);
    invocation.setOptLevel(options.optLevel);
    invocation.setOptPipeline(options.pipeline);
    invocation.generateDebugInfo(options.debug);
    timer.reset();
    return invocation.run();
}
