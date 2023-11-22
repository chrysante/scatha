#include <Catch/Catch2.hpp>

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
