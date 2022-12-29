#include "RegisterDescriptor.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace cg2;

assembly::RegisterIndex RegisterDescriptor::resolve(ir::Value const& value) {
    auto const [itr, success] = values.insert({ value.name(), index });
    if (success) { ++index; }
    return assembly::RegisterIndex(utl::narrow_cast<u8>(itr->second));
}

assembly::RegisterIndex RegisterDescriptor::makeTemporary() {
    SC_UNREACHABLE();
}
