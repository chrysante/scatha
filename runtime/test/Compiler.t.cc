#include <Catch/Catch2.hpp>

#include <scatha/Runtime/Compiler.h>
#include <scatha/Runtime/Executor.h>

using namespace scatha;

TEST_CASE("Simple function extraction") {
    Compiler compiler;
    compiler.addSourceText(R"(
export fn test(n: int, m: int) { return n + m; }
)");
    auto program = compiler.compile();
    auto exec = Executor::Make(std::move(program));
    auto testFn =
        exec->getFunction<int64_t(int64_t, int64_t)>("test-s64-s64").value();
    CHECK(testFn(1, 2) == 3);
}

TEST_CASE("Predeclared functions") {
    Compiler compiler;
    auto square = [](int64_t value) { return value * value; };
    auto squareDecl = compiler.declareFunction("square", square);
    int state = 0;
    auto stateful = [&] { ++state; };
    auto statefulDecl = compiler.declareFunction("stateful", stateful);
    compiler.addSourceText(R"(
export fn test(n: int) {
    stateful();
    return square(n);
})");
    auto program = compiler.compile();
    auto exec = Executor::Make(std::move(program));
    exec->addFunction(squareDecl, square);
    exec->addFunction(statefulDecl, stateful);
    auto testFn = exec->getFunction<int64_t(int64_t)>("test-s64").value();
    auto result = testFn(3);
    CHECK(result == 9);
    CHECK(state == 1);
}
