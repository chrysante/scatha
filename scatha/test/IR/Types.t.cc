#include <catch2/catch_test_macros.hpp>

#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

TEST_CASE("Structure size and align", "[ir]") {
    ir::Context ctx;
    ir::StructType Y("Y");
    Y.pushMember(ctx.intType(32));
    CHECK(Y.size() == 4);
    CHECK(Y.align() == 4);
    Y.pushMember(ctx.intType(32));
    CHECK(Y.size() == 8);
    CHECK(Y.align() == 4);
    Y.pushMember(ctx.intType(32));
    CHECK(Y.size() == 12);
    CHECK(Y.align() == 4);
    SECTION("1") {
        ir::StructType X("X");
        X.pushMember(ctx.intType(64));
        CHECK(X.size() == 8);
        CHECK(X.align() == 8);
        X.pushMember(&Y);
        CHECK(X.size() == 24);
        CHECK(X.align() == 8);
        X.pushMember(ctx.intType(8));
        CHECK(X.size() == 24);
        CHECK(X.align() == 8);
    }
    SECTION("2") {
        ir::StructType X("X");
        X.pushMember(ctx.intType(32));
        CHECK(X.size() == 4);
        CHECK(X.align() == 4);
        X.pushMember(&Y);
        CHECK(X.size() == 16);
        CHECK(X.align() == 4);
        X.pushMember(ctx.intType(8));
        CHECK(X.size() == 20);
        CHECK(X.align() == 4);
    }
}
