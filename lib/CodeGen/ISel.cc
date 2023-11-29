#include "CodeGen/ISel.h"

#include <range/v3/numeric.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISelCommon.h"
#include "CodeGen/ISelFunction.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

static size_t numParamRegisters(ir::Function const& F) {
    return ranges::accumulate(F.parameters(),
                              size_t(0),
                              ranges::plus{},
                              [&](auto& param) { return numWords(&param); });
}

static size_t numReturnRegisters(ir::Function const& F) {
    return numWords(F.returnType());
}

mir::Module cg::isel(ir::Module const& irMod) {
    ValueMap globalMap;
    mir::Module mirMod;
    utl::small_vector<std::pair<ir::Function const*, mir::Function*>> functions;
    for (auto& irFn: irMod) {
        auto* mirFn = new mir::Function(&irFn,
                                        numParamRegisters(irFn),
                                        numReturnRegisters(irFn),
                                        irFn.visibility());
        mirMod.addFunction(mirFn);
        functions.push_back({ &irFn, mirFn });
        globalMap.insert(&irFn, mirFn);
    }
    for (auto [irFn, mirFn]: functions) {
        iselFunction(*irFn, mirMod, *mirFn, globalMap);
    }
    return mirMod;
}
