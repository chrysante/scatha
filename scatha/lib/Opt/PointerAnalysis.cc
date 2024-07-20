#include "Opt/Passes.h"

#include <range/v3/view.hpp>
#include <svm/Builtin.h>
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

    utl::hashset<Value const*> provenanceVisited;
    utl::hashset<Value const*> escapingVisited;
    utl::hashset<Value const*> escaping;
    bool modified = false;

    void markEscaping(Value* value) { escaping.insert(value); }

    PtrAnalyzeCtx(Context& ctx, Function& function):
        ctx(ctx), function(function) {}

    bool run();

    /// Performs a DFS over the operands to analyze provenance
    void provenance(Value& inst);
    bool provenanceImpl(Alloca& inst);
    bool provenanceImpl(GetElementPointer& gep);
    bool provenanceImpl(ExtractValue& inst);
    bool provenanceImpl(Call& call);
    bool provenanceImpl(Value&) { return false; }

    /// Checks whether \p inst escapes its operands
    void analyzeEscapingOperands(Instruction& inst);
    void analyzeEscapingOperandsImpl(Call& call);
    void analyzeEscapingOperandsImpl(Store& store);
    void analyzeEscapingOperandsImpl(Instruction&) {}

    /// Performs a DFS over the users to analyze whether pointers escape
    void analyzeEscaping(Value& value);
    void analyzeEscapingImpl(GetElementPointer& inst);
    void analyzeEscapingImpl(InsertValue& inst);
    void analyzeEscapingImpl(ExtractValue& inst);
    void analyzeEscapingImpl(Value&) {}
};

} // namespace

bool opt::pointerAnalysis(Context& ctx, Function& function) {
    return PtrAnalyzeCtx(ctx, function).run();
}

bool PtrAnalyzeCtx::run() {
    for (auto& inst: function.instructions()) {
        provenance(inst);
        analyzeEscapingOperands(inst);
    }
    for (auto& param: function.parameters()) {
        analyzeEscaping(param);
    }
    for (auto& inst: function.instructions()) {
        analyzeEscaping(inst);
    }
    return modified;
}

void PtrAnalyzeCtx::provenance(Value& value) {
    if (value.pointerInfo()) {
        return;
    }
    if (!provenanceVisited.insert(&value).second) {
        return;
    }
    visit(value, [this](auto& value) { modified |= provenanceImpl(value); });
}

bool PtrAnalyzeCtx::provenanceImpl(Alloca& inst) {
    inst.allocatePointerInfo();
    inst.setPointerInfo(0, { .align = (ssize_t)inst.allocatedType()->align(),
                             .validSize = inst.allocatedSize(),
                             .provenance = PointerProvenance::Static(&inst),
                             .staticProvenanceOffset = 0,
                             .guaranteedNotNull = true });
    return true;
}

bool PtrAnalyzeCtx::provenanceImpl(ExtractValue& inst) {
    provenance(*inst.baseValue());
    if (inst.memberIndices().size() != 1) {
        return false;
    }
    size_t index = inst.memberIndices()[0];
    if (inst.baseValue()->ptrInfoArrayCount() <= index) {
        return false;
    }
    inst.allocatePointerInfo();
    inst.setPointerInfo(0, *inst.baseValue()->pointerInfo(index));
    return true;
}

static ssize_t mod(ssize_t a, ssize_t b) {
    SC_ASSERT(b > 0, "divisor must positive to compute modulo");
    ssize_t result = a % b;
    return result < 0 ? result + std::abs(b) : result;
}

static ssize_t computeAlign(ssize_t baseAlign, ssize_t offset) {
    if (baseAlign == 0) {
        return 0;
    }
    ssize_t r = mod(offset, baseAlign);
    return r == 0 ? baseAlign : r;
}

bool PtrAnalyzeCtx::provenanceImpl(GetElementPointer& gep) {
    provenance(*gep.basePointer());
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

bool PtrAnalyzeCtx::provenanceImpl(Call& call) {
    if (isBuiltinCall(&call, svm::Builtin::alloc)) {
        call.allocatePointerInfo(2);
        call.setPointerInfo(
            0, { /// We happen to know that all pointers returned by
                 /// `__builtin_alloc` are aligned to 16 byte boundaries
                 .align = 16,
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

void PtrAnalyzeCtx::analyzeEscapingOperands(Instruction& inst) {
    visit(inst, [&](auto& inst) { analyzeEscapingOperandsImpl(inst); });
}

void PtrAnalyzeCtx::analyzeEscapingOperandsImpl(Call& call) {
    /// Deallocation is not considered escaping
    if (isBuiltinCall(&call, svm::Builtin::dealloc)) {
        return;
    }
    for (auto* arg: call.arguments()) {
        markEscaping(arg);
    }
}

void PtrAnalyzeCtx::analyzeEscapingOperandsImpl(Store& store) {
    markEscaping(store.value());
}

void PtrAnalyzeCtx::analyzeEscaping(Value& value) {
    if (!value.pointerInfo()) {
        value.allocatePointerInfo();
    }
    if (!escapingVisited.insert(&value).second) {
        return;
    }
    for (auto* user: value.users()) {
        analyzeEscaping(*user);
    }
    if (!escaping.contains(&value)) {
        for (auto& info: value.pointerInfoRange()) {
            modified |= !info.isNonEscaping();
            info.setNonEscaping();
        }
    }
    visit(value, [this](auto& value) { analyzeEscapingImpl(value); });
}

void PtrAnalyzeCtx::analyzeEscapingImpl(GetElementPointer& inst) {
    if (escaping.contains(&inst)) {
        markEscaping(inst.basePointer());
    }
}

void PtrAnalyzeCtx::analyzeEscapingImpl(InsertValue& inst) {
    if (!escaping.contains(&inst)) {
        return;
    }
    for (auto* operand: inst.operands()) {
        markEscaping(operand);
    }
}

void PtrAnalyzeCtx::analyzeEscapingImpl(ExtractValue& inst) {
    if (escaping.contains(&inst)) {
        markEscaping(inst.baseValue());
    }
}
