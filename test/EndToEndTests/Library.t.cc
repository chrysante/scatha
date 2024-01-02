#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include "Assembly/Assembler.h"
#include "Invocation/CompilerInvocation.h"
#include "test/EndToEndTests/PassTesting.h"

using namespace scatha;
using namespace test;

static CompilerInvocation makeCompiler(std::stringstream& sstr) {
    CompilerInvocation inv;
    inv.setErrorStream(sstr);
    inv.setErrorHandler(
        [&] { throw std::runtime_error(std::move(sstr).str()); });
    return inv;
}

TEST_CASE("Static library compile and import", "[end-to-end][staticlib]") {
    std::stringstream sstr;
    CompilerInvocation lib1c = makeCompiler(sstr);
    lib1c.setInputs({ SourceFile::make(R"(
export fn inc(n: &mut int) {
    n += int(__builtin_sqrt_f64(1.0));
}
)") });
    lib1c.setOutputFile("libs/testlib1");
    lib1c.setTargetType(TargetType::StaticLibrary);
    lib1c.run();

    CompilerInvocation lib2c = makeCompiler(sstr);
    lib2c.setInputs({ SourceFile::make(R"(
import testlib1;

export fn incTwice(n: &mut int) {
    testlib1.inc(n);
    testlib1.inc(n);
}
)") });
    lib2c.setLibSearchPaths({ "libs" });
    lib2c.setOutputFile("libs/testlib2");
    lib2c.setTargetType(TargetType::StaticLibrary);
    lib2c.run();

    CompilerInvocation appc = makeCompiler(sstr);
    appc.setErrorStream(sstr);
    appc.setInputs({ SourceFile::make(R"(
import testlib2;
fn main() -> int {
    var n = 0;
    testlib2.incTwice(n);
    return n;
}
)") });
    appc.setLibSearchPaths({ "libs" });
    size_t startpos = 0;
    std::vector<uint8_t> program;
    appc.setCallbacks({ .asmCallback =
                            [&](Asm::AssemblerResult const& res) {
        startpos = findMain(res.symbolTable).value();
                       },
                        .linkerCallback =
                            [&](std::span<uint8_t const> data) {
        program.assign(data.begin(), data.end());
        appc.stop();
                        } });
    appc.run();
    CHECK(runProgram(program, startpos) == 2);
}
