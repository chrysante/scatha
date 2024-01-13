#include "Opt/Passes.h"

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/queue.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/PassRegistry.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"
#include "Opt/Common.h"

#include "IR/Print.h"
#include <iostream>

using namespace scatha;
using namespace opt;
using namespace ir;
using namespace ranges::views;

SC_REGISTER_PASS(opt::pointerAnalysis,
                 "pointeranalysis",
                 PassCategory::Experimental);

namespace {

struct PtrAnalyzeCtx {
    Context& ctx;
    Function& function;

    utl::hashset<Value const*> visited;
    bool modified = false;

    PtrAnalyzeCtx(Context& ctx, Function& function):
        ctx(ctx), function(function) {}

    bool run();

    void analyze(Value& inst);

    void analyzeImpl(Alloca& inst);
    void analyzeImpl(GetElementPointer& gep);
    void analyzeImpl(ExtractValue& inst);
    void analyzeImpl(Value&) {}
};

} // namespace

bool opt::pointerAnalysis(Context& ctx, Function& function) {
    return PtrAnalyzeCtx(ctx, function).run();
}

bool PtrAnalyzeCtx::run() {
    /// All instructions of type `ptr` are considered
    auto initial =
        function | join |
        filter([](auto& inst) { return isa<PointerType>(inst.type()); }) |
        TakeAddress | ToSmallVector<>;
    for (auto* inst: initial) {
        analyze(*inst);
    }
    return modified;
}

void PtrAnalyzeCtx::analyze(Value& value) {
    SC_EXPECT(isa<PointerType>(value.type()));
    if (value.pointerInfo()) {
        return;
    }
    if (!visited.insert(&value).second) {
        return;
    }
    std::cout << "Visiting " << format(value) << std::endl;
    visit(value, [this](auto& value) { analyzeImpl(value); });
}

void PtrAnalyzeCtx::analyzeImpl(Alloca& inst) {
    inst.allocatePointerInfo({ .align = inst.allocatedType()->align(),
                               .validSize = inst.allocatedSize(),
                               .provenance = &inst,
                               .staticProvenanceOffset = 0 });
}

void PtrAnalyzeCtx::analyzeImpl(ExtractValue& inst) {
    if (auto* call = dyncast<Call*>(inst.baseValue()); isBuiltinAlloc(call)) {
        inst.allocatePointerInfo(
            { .align = 16, /// We happen to know that all pointers returned by
                           /// `builtin.alloc` are align to 16 byte boundaries
              .validSize = std::nullopt,
              .provenance = call,
              .staticProvenanceOffset = 0 });
    }
}

void PtrAnalyzeCtx::analyzeImpl(GetElementPointer& gep) {
    analyze(*gep.basePointer());
}
