#include "Util/LibUtil.h"

#include <sstream>

#include "Assembly/Assembler.h"
#include "EndToEndTests/PassTesting.h"
#include "Invocation/CompilerInvocation.h"

using namespace scatha;
using namespace test;

static CompilerInvocation makeCompiler(std::stringstream& sstr) {
    CompilerInvocation inv;
    inv.setErrorStream(sstr);
    inv.setErrorHandler(
        [&] { throw std::runtime_error(std::move(sstr).str()); });
    return inv;
}

void test::compileLibrary(std::filesystem::path name,
                          std::filesystem::path libSearchPath,
                          std::string source) {
    std::stringstream sstr;
    CompilerInvocation inv = makeCompiler(sstr);
    inv.setInputs({ SourceFile::make(std::move(source)) });
    inv.setLibSearchPaths({ libSearchPath });
    inv.setOutputFile(name);
    inv.setTargetType(TargetType::StaticLibrary);
    inv.run();
}

uint64_t test::compileAndRunDependentProgram(
    std::filesystem::path libSearchPath, std::string source) {
    std::stringstream sstr;
    CompilerInvocation inv = makeCompiler(sstr);
    inv.setErrorStream(sstr);
    inv.setInputs({ SourceFile::make(std::move(source)) });
    inv.setLibSearchPaths({ libSearchPath });
    size_t startpos = 0;
    std::vector<uint8_t> program;
    auto asmCallback = [&](Asm::AssemblerResult const& res) {
        startpos = findMain(res.symbolTable).value();
    };
    auto linkerCallback = [&](std::span<uint8_t const> data) {
        program.assign(data.begin(), data.end());
        inv.stop();
    };
    inv.setCallbacks(
        { .asmCallback = asmCallback, .linkerCallback = linkerCallback });
    inv.run();
    return runProgram(program, startpos);
}