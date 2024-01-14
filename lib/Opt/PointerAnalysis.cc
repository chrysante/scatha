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

    bool analyzeImpl(Alloca& inst);
    bool analyzeImpl(GetElementPointer& gep);
    bool analyzeImpl(ExtractValue& inst);
    bool analyzeImpl(Value&) { return false; }
};

} // namespace

bool opt::pointerAnalysis(Context& ctx, Function& function) {
    return PtrAnalyzeCtx(ctx, function).run();
}

bool PtrAnalyzeCtx::run() {
    for (auto& inst: function.instructions()) {
        if (isa<PointerType>(inst.type())) {
            analyze(inst);
        }
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
    visit(value, [this](auto& value) { modified |= analyzeImpl(value); });
}

bool PtrAnalyzeCtx::analyzeImpl(Alloca& inst) {
    inst.allocatePointerInfo({ .align = inst.allocatedType()->align(),
                               .validSize = inst.allocatedSize(),
                               .provenance = &inst,
                               .staticProvenanceOffset = 0,
                               .guaranteedNotNull = true });
    return true;
}

bool PtrAnalyzeCtx::analyzeImpl(ExtractValue& inst) {
    if (auto* call = dyncast<Call*>(inst.baseValue()); isBuiltinAlloc(call)) {
        inst.allocatePointerInfo(
            { .align = 16, /// We happen to know that all pointers returned by
                           /// `builtin.alloc` are align to 16 byte boundaries
              .validSize = std::nullopt,
              .provenance = call,
              .staticProvenanceOffset = 0,
              .guaranteedNotNull = true });
        return true;
    }
    return false;
}

static size_t computeAlign(size_t baseAlign, size_t offset) {
    size_t r = offset % baseAlign;
    return r == 0 ? baseAlign : r;
}

bool PtrAnalyzeCtx::analyzeImpl(GetElementPointer& gep) {
    analyze(*gep.basePointer());
    auto* base = gep.basePointer()->pointerInfo();
    if (!base) {
        return false;
    }
    auto* accType = gep.accessedType();
    auto staticGEPOffset = gep.constantByteOffset();
    size_t align = [&] {
        if (staticGEPOffset) {
            return computeAlign(base->align(), *staticGEPOffset);
        }
        return std::min(base->align(), accType->align());
    }();
    SC_ASSERT(align != 0, "Align can never be zero");
    auto validSize = [&]() -> std::optional<size_t> {
        auto validBaseSize = base->validSize();
        if (!validBaseSize || !staticGEPOffset) return std::nullopt;
        return *validBaseSize - *staticGEPOffset;
        ;
    }();
    auto* provenance = base->provenance();
    auto staticProvOffset = [&]() -> std::optional<size_t> {
        auto baseOffset = base->staticProvencanceOffset();
        if (!baseOffset || !staticGEPOffset) return std::nullopt;
        return *baseOffset + *staticGEPOffset;
    }();
    bool guaranteedNotNull = base->guaranteedNotNull();
    gep.allocatePointerInfo(
        { align, validSize, provenance, staticProvOffset, guaranteedNotNull });
    return true;
}
