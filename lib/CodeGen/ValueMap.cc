#include "CodeGen/ValueMap.h"

#include "IR/CFG.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

mir::Value* ValueMap::operator()(ir::Value const* key) const {
    auto itr = map.find(key);
    SC_ASSERT(itr != map.end(), "Not found");
    return itr->second;
}

mir::Function* ValueMap::operator()(ir::Function const* key) const {
    return cast<mir::Function*>((*this)(static_cast<ir::Value const*>(key)));
}

void ValueMap::insert(ir::Value const* key, mir::Value* value) {
    auto [itr, success] = map.insert({ key, value });
    SC_ASSERT(success, "Key already present");
}
