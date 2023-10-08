#include "IRGen/FunctionGeneration.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IRGen/GlobalDecls.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;

utl::small_vector<sema::Function const*> irgen::generateFunction(
    sema::Function const& semaFn,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap) {
    if (semaFn.isNative()) {
        return generateAstFunction(*semaFn.definition(),
                                   irFn,
                                   ctx,
                                   mod,
                                   symbolTable,
                                   typeMap,
                                   functionMap);
    }
    else {
        SC_ASSERT(semaFn.isGenerated(), "");
        return generateSynthFunction(semaFn,
                                     irFn,
                                     ctx,
                                     mod,
                                     symbolTable,
                                     typeMap,
                                     functionMap);
    }
}

ir::Callable* FuncGenContextBase::getFunction(
    sema::Function const* semaFunction) {
    if (auto* irFunction = functionMap.tryGet(semaFunction)) {
        return irFunction;
    }
    if (semaFunction->isNative() || semaFunction->isGenerated()) {
        declaredFunctions.push_back(semaFunction);
    }
    return declareFunction(semaFunction, ctx, mod, typeMap, functionMap);
}

ir::ForeignFunction* FuncGenContextBase::getBuiltin(svm::Builtin builtin) {
    size_t index = static_cast<size_t>(builtin);
    auto* semaBuiltin = symbolTable.builtinFunction(index);
    auto* irBuiltin = getFunction(semaBuiltin);
    return cast<ir::ForeignFunction*>(irBuiltin);
}

void FuncGenContextBase::callMemcpy(ir::Value* dest,
                                    ir::Value* source,
                                    ir::Value* numBytes) {
    auto* memcpy = getBuiltin(svm::Builtin::memcpy);
    std::array args = { dest, numBytes, source, numBytes };
    add<ir::Call>(memcpy, args, std::string{});
}

void FuncGenContextBase::callMemcpy(ir::Value* dest,
                                    ir::Value* source,
                                    size_t numBytes) {
    callMemcpy(dest, source, ctx.intConstant(numBytes, 64));
}

CallingConvention const& FuncGenContextBase::getCC(
    sema::Function const* function) {
    return functionMap.metaData(function).CC;
}
