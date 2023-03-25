#include "Opt/InstCombine.h"

#include <utl/hashtable.hpp>

#include "IR/CFG.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

struct InstCombineCtx {
    InstCombineCtx(Context& irCtx, Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    bool visitInstruction(Instruction* inst);

    bool visitImpl(Instruction* inst) { return false; }
    bool visitImpl(ExtractValue* inst);
    bool visitImpl(InsertValue* inst);

    Context& irCtx;
    Function& function;
    utl::hashset<Instruction*> worklist;
};

} // namespace

bool opt::instCombine(Context& irCtx, Function& function) {
    InstCombineCtx ctx(irCtx, function);
    return ctx.run();
}

bool InstCombineCtx::run() {
    worklist = function.instructions() |
               ranges::views::transform([](auto& inst) { return &inst; }) |
               ranges::to<utl::hashset<Instruction*>>;
    bool modifiedAny = false;
    while (!worklist.empty()) {
        Instruction* inst = *worklist.begin();
        worklist.erase(worklist.begin());
        modifiedAny |= visitInstruction(inst);
    }
    return modifiedAny;
}

bool InstCombineCtx::visitInstruction(Instruction* inst) {
    return visit(*inst, [this](auto& inst) { return visitImpl(&inst); });
}

bool InstCombineCtx::visitImpl(ExtractValue* extractInst) {
    for (auto* insertInst = dyncast<InsertValue*>(extractInst->baseValue());
         insertInst != nullptr;
         insertInst = dyncast<InsertValue*>(insertInst->baseValue()))
    {
        if (ranges::equal(extractInst->memberIndices(),
                          insertInst->memberIndices()))
        {
            replaceValue(extractInst, insertInst->insertedValue());
            return true;
        }
    }
    return false;
}

bool InstCombineCtx::visitImpl(InsertValue* insertInst) {
#if 0
    utl::small_vector<InsertValue*> insts;
    for (; insertInst != nullptr; insertInst = dyncast<InsertValue*>(insertInst->baseValue())) {
        insts.push_back(insertInst);
    }
#endif
    return false;
}
