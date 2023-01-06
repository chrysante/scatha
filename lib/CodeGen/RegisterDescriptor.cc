#include "CodeGen/RegisterDescriptor.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace cg;
using namespace Asm;

Value RegisterDescriptor::resolve(ir::Value const& value) {
    if (auto* const constant = dyncast<ir::IntegralConstant const*>(&value)) {
        switch (constant->type()->size()) {
        case 1:
            return Value8(static_cast<u8>(constant->value()));
        case 2:
            return Value16(static_cast<u16>(constant->value()));
        case 4:
            return Value32(static_cast<u32>(constant->value()));
        case 8:
            return Value64(static_cast<u64>(constant->value()));
        default:
            SC_UNREACHABLE();
        }
    }
    else if (auto* constant = dyncast<ir::FloatingPointConstant const*>(&value)) {
        return Value64(static_cast<f64>(constant->value()));
    }
    SC_ASSERT(!value.name().empty(), "Name must not be empty.");
    auto const [itr, success] = values.insert({ value.name(), index });
    if (success) {
        index += utl::ceil_divide(value.type()->size(), 8);
    }
    return RegisterIndex(utl::narrow_cast<u8>(itr->second));
}

MemoryAddress RegisterDescriptor::resolveAddr(ir::Value const& address) {
    SC_ASSERT(address.type()->category() == ir::Type::Pointer, "address must be a pointer");
    auto const regIdx = resolve(address).get<RegisterIndex>().value();
    return MemoryAddress(regIdx);
}

RegisterIndex RegisterDescriptor::makeTemporary() {
    return RegisterIndex(index++);
}

RegisterIndex RegisterDescriptor::allocateAutomatic(size_t numRegisters) {
    RegisterIndex result(utl::narrow_cast<u8>(index));
    index += numRegisters;
    return result;
}
