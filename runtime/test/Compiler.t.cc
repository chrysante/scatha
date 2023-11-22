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
    auto callback = [](int64_t value) { return 2 * value; };
    auto decl = compiler.declareFunction("callback", callback);
    compiler.addSourceText(R"(
export fn test(n: int) { return callback(n); }
)");
    auto program = compiler.compile();
    auto exec = Executor::Make(std::move(program));
    exec->addFunction(decl, callback);
    auto testFn = exec->getFunction<int64_t(int64_t)>("test-s64").value();
    CHECK(testFn(2) == 4);
}
