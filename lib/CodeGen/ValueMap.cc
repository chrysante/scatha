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

void ValueMap::insert(ir::Value const* key, mir::Value* value) {
    auto [itr, success] = map.insert({ key, value });
    SC_ASSERT(success, "Key already present");
}
