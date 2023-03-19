#include "Volatile.h"

#include <iostream>

#include "Basic/Basic.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Parser/Parser.h"
#include "IR/Print.h"
#include "Opt/MemToReg.h"

using namespace scatha;

void playground::volatilePlayground() {

    auto const text = R"(
function i64 @f(i64) {
  %entry:
    %i.addr = alloca i64
    %res-ptr = alloca i64
    %j-ptr = alloca i64
    %tmp-ptr = alloca i64
    %tmp-ptr.1 = alloca i64
    store %i.addr, %0
    store %res-ptr, $0
    store %j-ptr, $1
    goto label %loop.header
  %loop.header:
    %j = load i64 %j-ptr
    %cmp.result = cmp leq i64 %j, i64 $5
    branch i1 %cmp.result, label %loop.body, label %loop.end
  %loop.body:
    %res = load i64 %res-ptr
    %expr.result = rem i64 %res, $2
    %cmp.result.1 = cmp eq i64 %expr.result, i64 $0
    branch i1 %cmp.result.1, label %then, label %else
  %loop.end:
    %res.3 = load i64 %res-ptr
    return i64 %res.3
  %then:
    %res.1 = load i64 %res-ptr
    %j.1 = load i64 %j-ptr
    %expr.result.1 = add i64 %res.1, %j.1
    store %tmp-ptr, %expr.result.1
    %tmp = load i64 %tmp-ptr
    store %res-ptr, %tmp
    %tmp.1 = load i64 %res-ptr
    goto label %if.end
  %else:
    %res.2 = load i64 %res-ptr
    %expr.result.2 = mul i64 $2, %res.2
    %j.2 = load i64 %j-ptr
    %expr.result.3 = sub i64 %expr.result.2, %j.2
    store %tmp-ptr.1, %expr.result.3
    %tmp.2 = load i64 %tmp-ptr.1
    store %res-ptr, %tmp.2
    %tmp.3 = load i64 %res-ptr
    goto label %if.end
  %if.end:
    %++.value.1 = load i64 %j-ptr
    %++.result = add i64 %++.value.1, $1
    store %j-ptr, %++.result
    goto label %loop.header
}
function i64 @main11() {
  %entry:
    %call.result = call i64 @f, i64 $4
    return i64 %call.result
})";

    ir::Context ctx;
    auto mod = ir::parse(text, ctx).value();

    auto& f = mod.functions().front();

    opt::memToReg(ctx, f);

    ir::print(mod);
}
