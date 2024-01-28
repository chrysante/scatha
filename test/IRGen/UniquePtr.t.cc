#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Util/FrontendWrapper.h"
#include "Util/IRTestUtils.h"

using namespace scatha;
using namespace test;

TEST_CASE("Unique expr of int", "[irgen]") {
    using namespace ir;
    std::string source = "public fn foo() -> int { return *(unique int(42)); }";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());

    auto entry = BBView(F.entry());
    auto* eight = ctx.intConstant(8, 64);
    auto& alloc = entry.nextAs<Call>();
    CHECK(alloc.function()->name() == "__builtin_alloc");
    CHECK(alloc.argumentAt(0) == eight);
    CHECK(alloc.argumentAt(1) == eight);
    auto& data = entry.nextAs<ExtractValue>();
    CHECK(data.baseValue() == &alloc);
    auto& store = entry.nextAs<Store>();
    CHECK(store.address() == &data);
    CHECK(store.value() == ctx.intConstant(42, 64));
    auto& load = entry.nextAs<Load>();
    CHECK(load.address() == &data);
    CHECK(load.type() == ctx.intType(64));

    auto del = entry.nextBlock();
    auto& dealloc = del.nextAs<Call>();
    CHECK(dealloc.function()->name() == "__builtin_dealloc");
    CHECK(dealloc.argumentAt(0) == &data);
    CHECK(dealloc.argumentAt(1) == eight);
    CHECK(dealloc.argumentAt(2) == eight);

    auto end = del.nextBlock();
    auto& ret = end.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("Unique expr of dynamic int array", "[irgen]") {
    using namespace ir;
    std::string source =
        "public fn foo() -> int { return (unique [int](42))[0]; }";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());

    auto entry = BBView(F.entry());
    auto& alloc = entry.nextAs<Call>();
    CHECK(alloc.function()->name() == "__builtin_alloc");
    CHECK(alloc.argumentAt(0) == ctx.intConstant(42 * 8, 64));
    auto& data = entry.nextAs<ExtractValue>();
    CHECK(data.baseValue() == &alloc);
    auto& memset = entry.nextAs<Call>();
    CHECK(memset.function()->name() == "__builtin_memset");
    CHECK(memset.argumentAt(0) == &data);
    CHECK(memset.argumentAt(1) == ctx.intConstant(42 * 8, 64));
    auto& gep = entry.nextAs<GetElementPointer>();
    CHECK(gep.basePointer() == &data);
    CHECK(gep.arrayIndex() == ctx.intConstant(0, 64));
    auto& load = entry.nextAs<Load>();
    CHECK(load.address() == &gep);
    CHECK(load.type() == ctx.intType(64));
    // Null pointer test
    auto& cmp = entry.nextAs<CompareInst>();
    CHECK(cmp.lhs() == &data);
    CHECK(cmp.rhs() == ctx.nullpointer());
    auto& branch = entry.nextAs<Branch>();
    CHECK(branch.condition() == &cmp);

    auto del = entry.nextBlock();
    auto& dealloc = del.nextAs<Call>();
    CHECK(dealloc.function()->name() == "__builtin_dealloc");
    CHECK(dealloc.argumentAt(0) == &data);
    auto& gt = del.nextAs<Goto>();

    auto end = del.nextBlock();
    auto& ret = end.nextAs<Return>();
    CHECK(ret.value() == &load);
}

TEST_CASE("Unique expr of array with nontrivial def ctor", "[irgen]") {
    using namespace ir;
    std::string source = R"(
struct X {
    fn new(&mut this) {}
}
public fn foo() {
    unique [X](10);
}
)";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());

    auto entry = BBView(F.entry());
    auto& alloc = entry.nextAs<Call>();
    CHECK(alloc.function()->name() == "__builtin_alloc");
    auto& data = entry.nextAs<ExtractValue>();
    CHECK(data.baseValue() == &alloc);
    auto& gotoBody = entry.nextAs<Goto>();

    auto body = entry.nextBlock();
    CHECK(gotoBody.target() == body.BB);
    CHECK_NOTHROW(body.nextAs<Phi>());
    CHECK_NOTHROW(body.nextAs<GetElementPointer>());
    CHECK_NOTHROW(body.nextAs<Call>());
    CHECK_NOTHROW(body.nextAs<ArithmeticInst>());
    CHECK_NOTHROW(body.nextAs<CompareInst>());
    CHECK_NOTHROW(body.nextAs<Branch>());

    auto constrEnd = body.nextBlock();
    auto del = constrEnd.nextBlock();
    auto& dealloc = del.nextAs<Call>();
    CHECK(dealloc.function()->name() == "__builtin_dealloc");
    CHECK(dealloc.argumentAt(0) == &data);

    auto delEnd = del.nextBlock();
    CHECK_NOTHROW(delEnd.nextAs<Return>());
}
