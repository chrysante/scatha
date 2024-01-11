#include "CodeGen/ValueMap.h"

using namespace scatha;
using namespace cg;

mir::Value* ValueMap::getValue(ir::Value const* key) const {
    auto itr = map.find(key);
    if (itr != map.end()) {
        return itr->second;
    }
    return nullptr;
}

void ValueMap::addValue(ir::Value const* key, mir::Value* value) {
    auto [itr, success] = map.insert({ key, value });
    SC_ASSERT(success, "Key already present");
}

std::pair<mir::Value*, size_t> ValueMap::getAddress(
    ir::Value const* key) const {
    auto itr = addressMap.find(key);
    if (itr != addressMap.end()) {
        return itr->second;
    }
    return {};
}

void ValueMap::addAddress(ir::Value const* key,
                          mir::Value* baseAddr,
                          size_t offset) {
    auto [itr, success] = addressMap.insert({ key, { baseAddr, offset } });
    SC_ASSERT(success, "Key already present");
}

std::optional<uint64_t> ValueMap::getStaticAddress(
    ir::Value const* value) const {
    auto itr = staticDataAddresses.find(value);
    if (itr != staticDataAddresses.end()) {
        return itr->second;
    }
    return std::nullopt;
}

void ValueMap::addStaticAddress(ir::Value const* key, uint64_t offset) {
    auto [itr, success] = staticDataAddresses.insert({ key, offset });
    SC_ASSERT(success, "Key already present");
}
