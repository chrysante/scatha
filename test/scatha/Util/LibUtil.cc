#include "Util/LibUtil.h"

#include <sstream>

#include "Assembly/Assembler.h"
#include "EndToEndTests/PassTesting.h"
#include "Invocation/CompilerInvocation.h"

using namespace scatha;
using namespace test;

static CompilerInvocation makeCompiler(TargetType targetType, std::string name,
                                       std::stringstream& sstr) {
    CompilerInvocation inv(targetType, name);
    inv.setErrorStream(sstr);
    inv.setErrorHandler(
        [&] { throw std::runtime_error(std::move(sstr).str()); });
    return inv;
}

void test::compileLibrary(std::filesystem::path name,
                          std::filesystem::path libSearchPath,
                          std::string source) {
    std::stringstream sstr;
    CompilerInvocation inv =
        makeCompiler(TargetType::StaticLibrary, name.stem().string(), sstr);
    inv.setInputs({ SourceFile::make(std::move(source)) });
    inv.setLibSearchPaths({ libSearchPath });
    auto target = inv.run();
    if (target) {
        target->writeToDisk(name.parent_path());
    }
}

uint64_t test::compileAndRunDependentProgram(
    std::filesystem::path libSearchPath, std::string source,
    Asm::LinkerOptions linkOptions) {
    std::stringstream sstr;
    CompilerInvocation inv = makeCompiler(TargetType::Executable, "test", sstr);
    inv.setErrorStream(sstr);
    inv.setInputs({ SourceFile::make(std::move(source)) });
    inv.setLibSearchPaths({ libSearchPath });
    inv.setLinkerOptions(linkOptions);
    size_t startpos = 0;
    auto asmCallback = [&](Asm::AssemblerResult const& res) {
        startpos = findMain(res.symbolTable).value();
    };
    inv.setCallbacks({ .asmCallback = asmCallback });
    auto target = inv.run();
    assert(target);
    return runProgram(target->binary(), startpos);
}
