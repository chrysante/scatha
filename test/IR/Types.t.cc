#include <Catch/Catch2.hpp>

#include "IR/Context.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

TEST_CASE("Structure size and align", "[ir]") {
    ir::Context ctx;
    ir::StructureType Y("Y");
    Y.addMember(ctx.integralType(32));
    CHECK(Y.size() == 4);
    CHECK(Y.align() == 4);
    Y.addMember(ctx.integralType(32));
    CHECK(Y.size() == 8);
    CHECK(Y.align() == 4);
    Y.addMember(ctx.integralType(32));
    CHECK(Y.size() == 12);
    CHECK(Y.align() == 4);
    SECTION("1") {
        ir::StructureType X("X");
        X.addMember(ctx.integralType(64));
        CHECK(X.size() == 8);
        CHECK(X.align() == 8);
        X.addMember(&Y);
        CHECK(X.size() == 24);
        CHECK(X.align() == 8);
        X.addMember(ctx.integralType(8));
        CHECK(X.size() == 24);
        CHECK(X.align() == 8);
    }
    SECTION("2") {
        ir::StructureType X("X");
        X.addMember(ctx.integralType(32));
        CHECK(X.size() == 4);
        CHECK(X.align() == 4);
        X.addMember(&Y);
        CHECK(X.size() == 16);
        CHECK(X.align() == 4);
        X.addMember(ctx.integralType(8));
        CHECK(X.size() == 20);
        CHECK(X.align() == 4);
    }
}
