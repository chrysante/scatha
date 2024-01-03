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

static void compileLibrary(std::filesystem::path name,
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

static uint64_t compileAndRunDependentProgram(
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

TEST_CASE("Static library compile and import", "[end-to-end][staticlib]") {
    compileLibrary("libs/testlib1",
                   "libs",
                   R"(
export fn inc(n: &mut int) {
    n += int(__builtin_sqrt_f64(1.0));
})");

    compileLibrary("libs/testlib2",
                   "libs",
                   R"(
import testlib1;
export fn incTwice(n: &mut int) {
    testlib1.inc(n);
    testlib1.inc(n);
})");

    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib2;
fn main() -> int {
    var n = 0;
    testlib2.incTwice(n);
    return n;
})");
    CHECK(ret == 2);
}

TEST_CASE("Import native lib in local scope", "[end-to-end][staticlib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    import testlib;
    return testlib.foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("Use native lib in local scope", "[end-to-end][staticlib]") {
    compileLibrary("libs/testlib", "libs", "export fn foo() { return 42; }");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
fn main() -> int {
    use testlib;
    return foo();
})");
    CHECK(ret == 42);
}

TEST_CASE("FFI library import", "[end-to-end][ffilib]") {
    SECTION("foo") {
        CHECK(42 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
fn main() {
    return foo(22, 20);
})"));
    }
    SECTION("bar") {
        test::runPrintsTest("bar(7, 11)\n", R"(
import "ffi-testlib";
extern "C" fn bar(n: int, m: int) -> void;
fn main() {
    bar(7, 11);
})");
    }
    SECTION("baz") {
        CHECK(42 == test::compileAndRun(R"(
import "ffi-testlib";
extern "C" fn baz() -> void;
fn main() {
    return baz();
})"));
    }
    SECTION("quux") {
        test::runPrintsTest("quux\n", R"(
import "ffi-testlib";
extern "C" fn quux() -> void;
fn main() {
    quux();
})");
    }
}

TEST_CASE("FFI used by static library", "[end-to-end][staticlib][ffilib]") {
    compileLibrary("libs/testlib",
                   "libs",
                   R"(
import "ffi-testlib";
extern "C" fn foo(n: int, m: int) -> int;
export fn fooWrapper(n: int, m: int) {
     return foo(n, m);
})");
    uint64_t ret = compileAndRunDependentProgram("libs",
                                                 R"(
import testlib;
fn main() -> int {
    return testlib.fooWrapper(20, 22);
})");
    CHECK(ret == 42);
}
