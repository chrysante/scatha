#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Equal.h"
#include "IR/IRParser.h"
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
    del.nextAs<Goto>();

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
    auto& dataEnd = entry.nextAs<GetElementPointer>();
    CHECK(dataEnd.basePointer() == &data);
    CHECK(dataEnd.arrayIndex() == ctx.intConstant(10, 64));
    auto& gotoBody = entry.nextAs<Goto>();

    auto body = entry.nextBlock();
    CHECK(gotoBody.target() == body.BB);
    CHECK_NOTHROW(body.nextAs<Phi>());
    CHECK_NOTHROW(body.nextAs<Call>());
    CHECK_NOTHROW(body.nextAs<GetElementPointer>());
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

TEST_CASE("Destruction of unique pointer to array function argument",
          "[irgen]") {
    auto [ctx, mod] = makeIR({ R"(
public struct Bar {
    fn delete(&mut this) {}
}
public fn foo(p: *unique [Bar]) {}
)" });
    auto [ctx2, ref] = ir::parse(R"(
struct @Bar {}

ext func void @__builtin_dealloc(ptr %0, i64 %1, i64 %2)

func void @Bar.delete-_R_MBar(ptr %0) {
  %entry:
    return
}

func void @foo-_U_A_MBar(ptr %0, i64 %1) {
  %entry:
    %p.addr = alloca { ptr, i64 }, i32 1
    %p.elem.0 = insert_value { ptr, i64 } undef, ptr %0, 0
    %p = insert_value { ptr, i64 } %p.elem.0, i64 %1, 1
    store ptr %p.addr, { ptr, i64 } %p
    %p.elem.0.addr = getelementptr inbounds { ptr, i64 }, ptr %p.addr, i32 0, 0
    %p.elem.1.addr = getelementptr inbounds { ptr, i64 }, ptr %p.addr, i32 0, 1
    %unique.ptr.data = load ptr, ptr %p.elem.0.addr
    %unique.ptr.engaged = ucmp neq ptr %unique.ptr.data, ptr nullptr
    // Destruction block for p
    branch i1 %unique.ptr.engaged, label %unique.ptr.delete, label %unique.ptr.end

  %unique.ptr.delete: // preds: entry
    %pointee.count = load i64, ptr %p.elem.1.addr
    // Destruction block for [Bar]
    %pointee.end = getelementptr inbounds @Bar, ptr %p.elem.0.addr, i64 %pointee.count
    goto label %pointee.destr.body

  %pointee.destr.body: // preds: unique.ptr.delete, pointee.destr.body
    %pointee.destr.counter = phi ptr [label %unique.ptr.delete : %p.elem.0.addr], [label %pointee.destr.body : %pointee.ind]
    // Destructor for Bar
    call void @Bar.delete-_R_MBar, ptr %pointee.destr.counter
    %pointee.ind = getelementptr inbounds @Bar, ptr %pointee.destr.counter, i64 1
    %pointee.destr.test = ucmp eq ptr %pointee.ind, ptr %pointee.end
    branch i1 %pointee.destr.test, label %pointee.destr.end, label %pointee.destr.body

  %pointee.destr.end: // preds: pointee.destr.body
    %unique.ptr.count = load i64, ptr %p.elem.1.addr
    %bytesize = mul i64 %unique.ptr.count, i64 1
    call void @__builtin_dealloc, ptr %unique.ptr.data, i64 %bytesize, i64 1
    goto label %unique.ptr.end

  %unique.ptr.end: // preds: entry, pointee.destr.end
    return
})")
                           .value();
    CHECK(test::modEqual(mod, ref));
}
