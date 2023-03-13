#include "IRSketch.h"

#include <iostream>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Print.h"

using namespace scatha;

void playground::irSketch() {
    ir::Context ctx;
    ir::Module mod;

    // The function
    ir::Type const* argTypes[] = { ctx.integralType(64) };
    auto* fn                   = new ir::Function(nullptr, ctx.integralType(64), argTypes, "fac");
    mod.addFunction(fn);

    // Entry block
    auto* entry = new ir::BasicBlock(ctx, "entry");
    fn->pushBack(entry);

    auto* allocaN = new ir::Alloca(ctx, ctx.integralType(64), "n_ptr");
    entry->pushBack(allocaN);

    auto* storeN5 = new ir::Store(ctx, allocaN, &fn->parameters().front());
    entry->pushBack(storeN5);

    auto* allocaI = new ir::Alloca(ctx, ctx.integralType(64), "i_ptr");
    entry->pushBack(allocaI);

    auto* storeI1 = new ir::Store(ctx, allocaN, ctx.integralConstant(1, 64));
    entry->pushBack(storeI1);

    auto* allocaResult = new ir::Alloca(ctx, ctx.integralType(64), "result_ptr");
    entry->pushBack(allocaResult);

    auto* storeResult1 = new ir::Store(ctx, allocaResult, ctx.integralConstant(1, 64));
    entry->pushBack(storeResult1);

    auto* loopHeader = new ir::BasicBlock(ctx, "loop_header");
    fn->pushBack(loopHeader);

    auto* gotoLoopHeader = new ir::Goto(ctx, loopHeader);
    entry->pushBack(gotoLoopHeader);

    // Loop header block
    auto* loadI1 = new ir::Load(allocaI, "i1");
    loopHeader->pushBack(loadI1);

    auto* loadN1 = new ir::Load(allocaN, "n1");
    loopHeader->pushBack(loadN1);

    auto* cmp = new ir::CompareInst(ctx, loadI1, loadN1, ir::CompareOperation::LessEq, "loop_cond");
    loopHeader->pushBack(cmp);

    auto* loopBody = new ir::BasicBlock(ctx, "loop_body");
    fn->pushBack(loopBody);

    auto* end = new ir::BasicBlock(ctx, "end");
    fn->pushBack(end);

    auto* lhBranch = new ir::Branch(ctx, cmp, loopBody, end);
    loopHeader->pushBack(lhBranch);

    // Loop body block - decl
    auto* loadResult1 = new ir::Load(allocaResult, "result1");
    loopBody->pushBack(loadResult1);

    auto* loadI2 = new ir::Load(allocaI, "i2");
    loopBody->pushBack(loadI2);

    auto* mulTmp = new ir::ArithmeticInst(loadResult1, loadI2, ir::ArithmeticOperation::Mul, "mul-tmp");
    loopBody->pushBack(mulTmp);

    auto storeResult2 = new ir::Store(ctx, allocaResult, mulTmp);
    loopBody->pushBack(storeResult2);

    auto* addTmp = new ir::ArithmeticInst(loadI2, ctx.integralConstant(1, 64), ir::ArithmeticOperation::Add, "add-tmp");
    loopBody->pushBack(addTmp);

    auto* storeI2 = new ir::Store(ctx, allocaI, addTmp);
    loopBody->pushBack(storeI2);

    auto* gotoLoopHeader2 = new ir::Goto(ctx, loopHeader);
    loopBody->pushBack(gotoLoopHeader2);

    // End block - decl
    auto* loadResult2 = new ir::Load(allocaResult, "result2");
    end->pushBack(loadResult2);

    auto* ret = new ir::Return(ctx, loadResult2);
    end->pushBack(ret);

    std::cout << std::endl;

    ir::print(mod);

    std::cout << std::endl;
}
