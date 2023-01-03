#include "CodeGen2/RegisterDescriptor.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace cg2;
using namespace asm2;

Value RegisterDescriptor::resolve(ir::Value const& value) {
    if (auto* constant = dyncast<ir::IntegralConstant const*>(&value)) {
        return Value64(static_cast<u64>(constant->value()));
    }
    else if (auto* constant = dyncast<ir::FloatingPointConstant const*>(&value)) {
        return Value64(static_cast<f64>(constant->value()));
    }
    SC_ASSERT(!value.name().empty(), "Name must not be empty.");
    auto const [itr, success] = values.insert({ value.name(), index });
    if (success) {
        ++index;
    }
    return RegisterIndex(utl::narrow_cast<u8>(itr->second));
}

MemoryAddress RegisterDescriptor::resolveAddr(ir::Value const& address) {
    SC_ASSERT(address.type()->category() == ir::Type::Pointer,
              "address must be a pointer");
    auto const regIdx = resolve(address).get<RegisterIndex>().value();
    return MemoryAddress(regIdx, 0, 0);
}

RegisterIndex RegisterDescriptor::makeTemporary() {
    return RegisterIndex(index++);
}

RegisterIndex RegisterDescriptor::allocateAutomatic(size_t numRegisters) {
    RegisterIndex result(utl::narrow_cast<u8>(index));
    index += numRegisters;
    return result;
}
