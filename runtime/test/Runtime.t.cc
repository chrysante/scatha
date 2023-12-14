#include <catch2/catch_test_macros.hpp>

#include <scatha/Runtime/LibSupport.h>
#include <scatha/Runtime/Runtime.h>

using namespace scatha;

TEST_CASE("Runtime") {
    Runtime runtime;
    runtime.addSourceText("export fn f() { func(10); }");
    int state = 0;
    auto func = [&](int n) { state += n; };
    runtime.addFunction("func", func);
    runtime.compile();
    runtime.getFunction<void()>("f").value()();
    CHECK(state == 10);
}

static int64_t globalCallback(int64_t n) { return 3 * n; }

TEST_CASE("Function pointer") {
    Runtime runtime;
    runtime.addSourceText("export fn f(n: int) { return callback(n); }");
    runtime.addFunction("callback", globalCallback);
    runtime.compile();
    auto value = runtime.getFunction<int64_t(int64_t)>("f-s64").value()(2);
    CHECK(value == 6);
}

static void myFunction() {}

SC_EXPORT_FUNCTION(myFunction, "myFunction");
