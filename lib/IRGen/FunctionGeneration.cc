#include "IRGen/FunctionGeneration.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/InvariantSetup.h"
#include "IR/Validate.h"
#include "IRGen/GlobalDecls.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;

void irgen::generateFunction(Config config, FuncGenParameters params) {
    if (params.semaFn.isNative()) {
        generateAstFunction(config, params);
    }
    else {
        SC_EXPECT(params.semaFn.isGenerated());
        generateSynthFunction(config, params);
    }
    ir::setupInvariants(params.ctx, params.irFn);
    ir::assertInvariants(params.ctx, params.irFn);
}

ir::Callable* FuncGenContextBase::getFunction(
    sema::Function const* semaFunction) {
    if (auto* irFunction = functionMap.tryGet(semaFunction)) {
        return irFunction;
    }
    if (semaFunction->isNative() || semaFunction->isGenerated()) {
        declQueue.push_back(semaFunction);
    }
    return declareFunction(semaFunction, ctx, mod, typeMap, functionMap);
}

ir::ForeignFunction* FuncGenContextBase::getBuiltin(svm::Builtin builtin) {
    size_t index = static_cast<size_t>(builtin);
    auto* semaBuiltin = symbolTable.builtinFunction(index);
    auto* irBuiltin = getFunction(semaBuiltin);
    return cast<ir::ForeignFunction*>(irBuiltin);
}

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest,
                                         ir::Value* source,
                                         ir::Value* numBytes) {
    auto* memcpy = getBuiltin(svm::Builtin::memcpy);
    std::array args = { dest, numBytes, source, numBytes };
    return add<ir::Call>(memcpy, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemcpy(ir::Value* dest,
                                         ir::Value* source,
                                         size_t numBytes) {
    return callMemcpy(dest, source, ctx.intConstant(numBytes, 64));
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest,
                                         ir::Value* numBytes,
                                         int value) {
    auto* memset = getBuiltin(svm::Builtin::memset);
    ir::Value* irVal = ctx.intConstant(utl::narrow_cast<uint64_t>(value), 64);
    std::array args = { dest, numBytes, irVal };
    return add<ir::Call>(memset, args, std::string{});
}

ir::Call* FuncGenContextBase::callMemset(ir::Value* dest,
                                         size_t numBytes,
                                         int value) {
    return callMemset(dest, ctx.intConstant(numBytes, 64), value);
}

CallingConvention const& FuncGenContextBase::getCC(
    sema::Function const* function) {
    return functionMap.metaData(function).CC;
}
