#include <iostream>
#include <iomanip>

#include "Common/APFloat.h"

#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Function.h"
#include "IR/BasicBlock.h"
#include "IR/Instruction.h"
#include "IR/Parameter.h"
#include "IR/Print.h"

using namespace scatha;

static void printAPFloat(std::string_view name, APFloat const& f) {
    std::cout << name << ":\n";
    std::cout << "\tExponent:       " << f.exponent() << "\n";
    std::cout << "\tPrecision:      " << f.precision() << "\n";
    std::cout << "\tMantissa limbs: " << f.mantissa().size() << "\n";
    if (!f.mantissa().empty()) {
        std::cout << "\tMantissa:       ";
        for (bool first = true; auto i: f.mantissa()) {
            if (!first) { std::cout << "\t                "; } else { first = false; }
            std::cout << std::bitset<64>(i) << std::endl;
        }
    }
    std::cout << "\tValue:          " << f << "\n";
}

[[gnu::weak]]
int main() {
    
    ir::Context ctx;
    
    ir::Module mod;
    
    using namespace std::literals;
    
    // The function
    ir::Type const* argTypes[] = { ctx.integralType(64) };
    auto* fn = new ir::Function(nullptr, ctx.integralType(64), argTypes, "fac"sv);
    mod.addFunction(fn);

    // Entry block
    auto* entry = new ir::BasicBlock(ctx, "entry");
    fn->addBasicBlock(entry);
    
    auto* allocaN = new ir::Alloca(ctx, ctx.integralType(64), "n_ptr");
    entry->addInstruction(allocaN);
    
    auto* storeN5 = new ir::Store(ctx, allocaN, &fn->parameters().front());
    entry->addInstruction(storeN5);
    
    auto* allocaI = new ir::Alloca(ctx, ctx.integralType(64), "i_ptr");
    entry->addInstruction(allocaI);
    
    auto* storeI1 = new ir::Store(ctx, allocaN, ctx.integralConstant(1, 64));
    entry->addInstruction(storeI1);
    
    auto* allocaResult = new ir::Alloca(ctx, ctx.integralType(64), "result_ptr");
    entry->addInstruction(allocaResult);
    
    auto* storeResult1 = new ir::Store(ctx, allocaResult, ctx.integralConstant(1, 64));
    entry->addInstruction(storeResult1);
    
    auto* loopHeader = new ir::BasicBlock(ctx, "loop_header");
    fn->addBasicBlock(loopHeader);
    
    auto* gotoLoopHeader = new ir::Goto(ctx, loopHeader);
    entry->addInstruction(gotoLoopHeader);
    
    // Loop header block
    auto* loadI1 = new ir::Load(ctx.integralType(64), allocaI, "i1");
    loopHeader->addInstruction(loadI1);
    
    auto* loadN1 = new ir::Load(ctx.integralType(64), allocaN, "n1");
    loopHeader->addInstruction(loadN1);

    auto* cmp = new ir::CompareInst(ctx, loadI1, loadN1, ir::CompareOperation::LessEq, "loop_cond");
    loopHeader->addInstruction(cmp);
    
    auto* loopBody = new ir::BasicBlock(ctx, "loop_body");
    fn->addBasicBlock(loopBody);
    
    auto* end = new ir::BasicBlock(ctx, "end");
    fn->addBasicBlock(end);
    
    auto* lhBranch = new ir::Branch(ctx, cmp, loopBody, end);
    loopHeader->addInstruction(lhBranch);
    
    // Loop body block - decl
    auto* loadResult1 = new ir::Load(ctx.integralType(64), allocaResult, "result1");
    loopBody->addInstruction(loadResult1);

    auto* loadI2 = new ir::Load(ctx.integralType(64), allocaI, "i2");
    loopBody->addInstruction(loadI2);
    
    auto* mulTmp = new ir::ArithmeticInst(loadResult1, loadI2, ir::ArithmeticOperation::Mul, "mul-tmp");
    loopBody->addInstruction(mulTmp);
    
    auto storeResult2 = new ir::Store(ctx, allocaResult, mulTmp);
    loopBody->addInstruction(storeResult2);
    
    auto* addTmp = new ir::ArithmeticInst(loadI2, ctx.integralConstant(1, 64), ir::ArithmeticOperation::Add, "add-tmp");
    loopBody->addInstruction(addTmp);
    
    auto* storeI2 = new ir::Store(ctx, allocaI, addTmp);
    loopBody->addInstruction(storeI2);
    
    auto* gotoLoopHeader2 = new ir::Goto(ctx, loopHeader);
    loopBody->addInstruction(gotoLoopHeader2);
    
    // End block - decl
    auto* loadResult2 = new ir::Load(ctx.integralType(64), allocaResult, "result2");
    end->addInstruction(loadResult2);
    
    auto* ret = new ir::Return(ctx, loadResult2);
    end->addInstruction(ret);
    
    std::cout << std::endl;
    
    ir::print(mod);
    
    std::cout << std::endl;
    
}
