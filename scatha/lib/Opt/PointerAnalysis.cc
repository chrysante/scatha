#include "Opt/Passes.h"

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/queue.hpp>
#include <utl/vector.hpp>

#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
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

SC_REGISTER_PASS(opt::pointerAnalysis, "ptranalysis", PassCategory::Analysis);

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
    bool analyzeImpl(Call& call);
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
    if (value.pointerInfo()) {
        return;
    }
    if (!visited.insert(&value).second) {
        return;
    }
    visit(value, [this](auto& value) { modified |= analyzeImpl(value); });
}

bool PtrAnalyzeCtx::analyzeImpl(Alloca& inst) {
    inst.allocatePointerInfo();
    inst.setPointerInfo(0, { .align = (ssize_t)inst.allocatedType()->align(),
                             .validSize = inst.allocatedSize(),
                             .provenance = PointerProvenance::Static(&inst),
                             .staticProvenanceOffset = 0,
                             .guaranteedNotNull = true });
    return true;
}

bool PtrAnalyzeCtx::analyzeImpl(ExtractValue& inst) {
    analyze(*inst.baseValue());
    if (inst.memberIndices().size() != 1) {
        return false;
    }
    size_t index = inst.memberIndices()[0];
    if (inst.baseValue()->ptrInfoArrayCount() <= index) {
        return false;
    }
    inst.allocatePointerInfo();
    inst.setPointerInfo(index, *inst.baseValue()->pointerInfo(index));
    return true;
}

static ssize_t mod(ssize_t a, ssize_t b) {
    SC_ASSERT(b > 0, "divisor must positive to compute modulo");
    ssize_t result = a % b;
    return result < 0 ? result + std::abs(b) : result;
}

static ssize_t computeAlign(ssize_t baseAlign, ssize_t offset) {
    ssize_t r = mod(offset, baseAlign);
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
    ssize_t align = [&] {
        if (staticGEPOffset) {
            return computeAlign(base->align(), *staticGEPOffset);
        }
        return std::min(base->align(), (ssize_t)accType->align());
    }();
    SC_ASSERT(align != 0, "Align can never be zero");
    auto validSize = [&]() -> std::optional<size_t> {
        auto validBaseSize = base->validSize();
        if (!validBaseSize || !staticGEPOffset) return std::nullopt;
        return utl::narrow_cast<size_t>((ssize_t)*validBaseSize -
                                        *staticGEPOffset);
    }();
    auto provenance = base->provenance();
    auto staticProvOffset = [&]() -> std::optional<size_t> {
        auto baseOffset = base->staticProvencanceOffset();
        if (!baseOffset || !staticGEPOffset) return std::nullopt;
        return utl::narrow_cast<size_t>((ssize_t)*baseOffset +
                                        *staticGEPOffset);
    }();
    bool guaranteedNotNull = base->guaranteedNotNull();
    gep.allocatePointerInfo();
    gep.setPointerInfo(0, { .align = align,
                            .validSize = validSize,
                            .provenance = provenance,
                            .staticProvenanceOffset = staticProvOffset,
                            .guaranteedNotNull = guaranteedNotNull });
    return true;
}

bool PtrAnalyzeCtx::analyzeImpl(Call& call) {
    if (isBuiltinAlloc(&call)) {
        call.allocatePointerInfo(2);
        call.setPointerInfo(
            0,
            { .align = 16, /// We happen to know that all pointers returned by
              /// `__builtin_alloc` are aligned to 16 byte boundaries
              .validSize = std::nullopt,
              .provenance = PointerProvenance::Static(&call),
              .staticProvenanceOffset = 0,
              .guaranteedNotNull = true });
        return true;
    }
    if (isa<PointerType>(call.type())) {
        call.allocatePointerInfo();
        call.setPointerInfo(0,
                            { .align = 0,
                              .validSize = std::nullopt,
                              .provenance = PointerProvenance::Dynamic(&call),
                              .staticProvenanceOffset = 0 });
        return true;
    }
    return false;
}
