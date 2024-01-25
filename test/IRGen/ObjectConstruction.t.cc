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

TEST_CASE("Non-trivial aggregate construction", "[irgen]") {
    using namespace ir;
    std::string source = R"(
struct Nontrivial {
    fn new(&mut this, n: int) {}
    fn new(&mut this, rhs: &Nontrivial) {}
    fn delete(&mut this) {}
}
struct Aggr {
    var n: Nontrivial;
    var i: int;
}
public fn foo() {
    let value = Aggr(Nontrivial(1), 2);
}
)";
    auto [ctx, mod] = makeIR({ source });
    auto& F = mod.front();
    CHECK(F.parameters().empty());
    auto view = BBView(F.entry());

    auto& mem = view.nextAs<Alloca>();
    auto& nontrivAddr = view.nextAs<GetElementPointer>();
    CHECK(nontrivAddr.basePointer() == &mem);
    auto& nonTrivCtorCall = view.nextAs<Call>();
    CHECK(nonTrivCtorCall.argumentAt(0) == &nontrivAddr);
    CHECK(nonTrivCtorCall.argumentAt(1) == ctx.intConstant(1, 64));

    auto& intAddr = view.nextAs<GetElementPointer>();
    CHECK(intAddr.basePointer() == &mem);
    auto& intStore = view.nextAs<Store>();
    CHECK(intStore.address() == &intAddr);
    CHECK(intStore.value() == ctx.intConstant(2, 64));
    auto& dtorCall = view.nextAs<Call>();
    CHECK(dtorCall.argumentAt(0) == &mem);
    CHECK_NOTHROW(view.nextAs<Return>());
}
