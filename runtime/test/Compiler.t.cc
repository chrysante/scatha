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

TEST_CASE("Nested VM invocations") {
    Compiler compiler;
    std::function<int64_t(int64_t)> callback;
    auto cbDecl = compiler.declareFunction("callback", callback);
    compiler.addSourceText(R"(
export fn f(n: int) {
    return callback(n + 1);
}
fn fac(n: int) -> int {
    return n <= 1 ? 1 : n * fac(n - 1);
}
export fn g(n: int) {
    return fac(n);
}
)");
    auto executor = Executor::Make(compiler.compile());
    executor->addFunction(cbDecl, callback);
    callback = executor->getFunction<int64_t(int64_t)>("g-s64").value();
    auto f = executor->getFunction<int64_t(int64_t)>("f-s64").value();
    auto result = f(3);
    CHECK(result == 24);
}

TEST_CASE("Struct declaration") {
    /// `Compiler::declareType()` is not implemented yet
#if 0
    Compiler compiler;
    struct MyStruct {
        int64_t foo;
        double bar;
    };
    compiler.mapType(typeid(MyStruct), { "MyStruct", { { "foo", typeid(int64_t) }, { "bar", typeid(double) } } });
    auto myFunction = [](MyStruct s) { return s.foo + static_cast<int64_t>(s.bar); };
    auto myFunctionDecl = compiler.declareFunction("myFunction", myFunction);
    compiler.addSourceText(R"(
export fn f(s: MyStruct) {
    return myFunction(s) + s.foo;
}
)");
    auto executor = Executor::Make(compiler.compile());
    executor->addFunction(myFunctionDecl, myFunction);
    auto f = executor->getFunction<int64_t(MyStruct)>("f-MyStruct").value();
    auto result = f({ .foo = 3, .bar = 1.5 });
    CHECK(result == 24);
#endif
}
