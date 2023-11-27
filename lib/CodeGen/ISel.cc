#include "CodeGen/ISel.h"

#include <utl/vector.hpp>

#include "CodeGen/ISelFunction.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

mir::Module cg::isel(ir::Module const& irMod) {
    ValueMap globalMap;
    mir::Module mirMod;
    utl::small_vector<std::pair<ir::Function const*, mir::Function*>> functions;
    for (auto& irFn: irMod) {
        auto* mirFn = new mir::Function(&irFn,
                                        0,
                                        0, // For now!
                                        irFn.visibility());
        mirMod.addFunction(mirFn);
        functions.push_back({ &irFn, mirFn });
        globalMap.insert(&irFn, mirFn);
    }
    for (auto [irFn, mirFn]: functions) {
        iselFunction(*irFn, *mirFn, globalMap);
    }
    return mirMod;
}
