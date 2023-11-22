#include <Catch/Catch2.hpp>

#include <scatha/Runtime/Compiler.h>
#include <scatha/Runtime/Executor.h>

using namespace scatha;

TEST_CASE("Compiler") {
    Compiler compiler;
    compiler.addSourceText(R"(
export fn test(n: int, m: int) { return n + m; }
)");
    auto program = compiler.compile();
    auto exec = Executor::Make(program);
    auto testFn = exec->getFunction<int(int, int)>("test-s64-s64").value();
    CHECK(testFn(1, 2) == 3);
}
