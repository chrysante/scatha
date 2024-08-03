#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_all.hpp>
#include <svm/VirtualMachine.h>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "CodeGen/CodeGen.h"
#include "IR/Context.h"
#include "IR/ForEach.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "Opt/Passes.h"

using namespace scatha;

static uint64_t run(ir::Module const& mod) {
    auto obj = Asm::assemble(cg::codegen(mod));
    auto linkresult = Asm::link({}, obj.program, {}, obj.unresolvedSymbols);
    REQUIRE(linkresult);
    svm::VirtualMachine VM;
    VM.loadBinary(obj.program.data());
    VM.execute({});
    return VM.getRegister(0);
}

TEST_CASE("Loop rotate bug", "[looprotate]") {
    auto text = R"(
func i64 @main() {
  %entry:
    goto label %loop.header
    
  %loop.header: // preds: entry, if.end
    %i.addr.0 = phi i64 [label %entry : 0], [label %if.end : %++.res]
    %ls = scmp ls i64 %i.addr.0, i64 5
    branch i1 %ls, label %loop.body, label %loop.end
    
  %loop.body: // preds: loop.header
    %grteq = scmp geq i64 %i.addr.0, i64 3
    branch i1 %grteq, label %if.then, label %if.end
        
  %if.then: // preds: loop.body
    goto label %loop.end

  %if.end: // preds: loop.body
    %++.res = add i64 %i.addr.0, i64 1
    goto label %loop.header

  %loop.end: // preds: loop.header, if.then
    return i64 %i.addr.0
}
)";
    auto [ctx, mod] = ir::parse(text).value();
    CHECK(run(mod) == 3);
    ir::forEach(ctx, mod, opt::loopRotate, {});
    CHECK(run(mod) == 3);
}

TEST_CASE("Break from nested loop", "[looprotate]") {
    int argument = GENERATE(0, 1, 42, 100);
    int expectedResult = argument < 100 ? argument : 0;
    auto text = R"(
func i64 @main() {
  %entry:
    %call.result = call i64 @testFn-s64, i64 )" +
                std::to_string(argument) + R"(
    return i64 %call.result
}

func i64 @testFn-s64(i64 %0) {
  %entry:
    goto label %loop.header

  %loop.header: // preds: entry, loop.inc.0
    %total.addr.0 = phi i64 [label %entry : 0], [label %loop.inc.0 : %total.addr.1]
    %i.addr.0 = phi i64 [label %entry : 0], [label %loop.inc.0 : %++.res.1]
    %ls = scmp ls i64 %i.addr.0, i64 10
    branch i1 %ls, label %loop.body, label %loop.end.0

  %loop.body: // preds: loop.header
    goto label %loop.header.0

  %loop.header.0: // preds: loop.body, loop.inc
    %total.addr.1 = phi i64 [label %loop.body : %total.addr.0], [label %loop.inc : %++.res.0]
    %j.addr.0 = phi i64 [label %loop.body : 0], [label %loop.inc : %++.res]
    %ls.0 = scmp ls i64 %j.addr.0, i64 10
    branch i1 %ls.0, label %loop.body.0, label %loop.end

  %loop.body.0: // preds: loop.header.0
    %eq = scmp eq i64 %total.addr.1, i64 %0
    branch i1 %eq, label %if.then, label %if.end

  %if.then: // preds: loop.body.0
    goto label %return

  %if.end: // preds: loop.body.0
    goto label %loop.inc

  %loop.inc: // preds: if.end
    %++.res = add i64 %j.addr.0, i64 1
    %++.res.0 = add i64 %total.addr.1, i64 1
    goto label %loop.header.0

  %loop.end: // preds: loop.header.0
    goto label %loop.inc.0

  %loop.inc.0: // preds: loop.end
    %++.res.1 = add i64 %i.addr.0, i64 1
    goto label %loop.header

  %loop.end.0: // preds: loop.header
    goto label %return

  %return: // preds: if.then, loop.end.0
    %retval = phi i64 [label %if.then : %total.addr.1], [label %loop.end.0 : 0]
    return i64 %retval
}
)";
    auto [ctx, mod] = ir::parse(text).value();
    CHECK(run(mod) == (uint64_t)expectedResult);
    ir::forEach(ctx, mod, opt::loopRotate, {});
    CHECK(run(mod) == (uint64_t)expectedResult);
}

TEST_CASE("GCD(1253, 756476)", "[looprotate]") {
    auto text = R"(
func i64 @main() {
  %entry:
    goto label %log.end

  %log.end: // preds: entry, loop.body
    %b.addr.1 = phi i64 [label %entry : 1253], [label %loop.body : %rem]
    %a.addr.1 = phi i64 [label %entry : 756476], [label %loop.body : %b.addr.1]
    %neq.0 = scmp neq i64 %b.addr.1, i64 0
    branch i1 %neq.0, label %loop.body, label %log.end.0

  %loop.body: // preds: log.end
    %rem = srem i64 %a.addr.1, i64 %b.addr.1
    goto label %log.end

  %log.end.0: // preds: log.end
    %res = phi i64 [label %log.end : %a.addr.1]
    return i64 %res
}
)";
    auto [ctx, mod] = ir::parse(text).value();
    CHECK(run(mod) == 7);
    ir::forEach(ctx, mod, opt::loopRotate, {});
    CHECK(run(mod) == 7);
}

TEST_CASE("GCD(1253, 756476) + GCD(7, 1)", "[looprotate]") {
    auto text = R"(
func i64 @main() {
  %entry:
    goto label %log.end

  %log.end: // preds: entry, loop.body
    %b.addr.1 = phi i64 [label %entry : 1253], [label %loop.body : %rem]
    %a.addr.1 = phi i64 [label %entry : 756476], [label %loop.body : %b.addr.1]
    %neq.0 = scmp neq i64 %b.addr.1, i64 0
    branch i1 %neq.0, label %loop.body, label %log.end.0

  %loop.body: // preds: log.end
    %rem = srem i64 %a.addr.1, i64 %b.addr.1
    goto label %log.end

  %log.end.0: // preds: log.end, loop.body.0
    %b.addr.2 = phi i64 [label %log.end : 7], [label %loop.body.0 : %rem.0]
    %a.addr.2 = phi i64 [label %log.end : 1], [label %loop.body.0 : %b.addr.2]
    %neq.1 = scmp neq i64 %b.addr.2, i64 0
    branch i1 %neq.1, label %loop.body.0, label %loop.end.0

  %loop.body.0: // preds: log.end.0
    %rem.0 = srem i64 %a.addr.2, i64 %b.addr.2
    goto label %log.end.0

  %loop.end.0: // preds: log.end.0
    %sum.2 = add i64 %a.addr.1, i64 %a.addr.2
    return i64 %sum.2
}
)";
    auto [ctx, mod] = ir::parse(text).value();
    CHECK(run(mod) == 8);
    ir::forEach(ctx, mod, opt::loopRotate, {});
    CHECK(run(mod) == 8);
}
