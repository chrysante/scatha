#include "RegisterDescriptor.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace cg2;

std::variant<assembly::RegisterIndex, assembly::MemoryAddress, assembly::Value64> RegisterDescriptor::resolve(ir::Value const& value) {
    if (auto* constant = dyncast<ir::IntegralConstant const*>(&value)) {
        return assembly::Value64(static_cast<u64>(constant->value()));
    }
    SC_ASSERT(!value.name().empty(), "Name must not be empty.");
    auto const [itr, success] = values.insert({ value.name(), index });
    if (success) { ++index; }
    return assembly::RegisterIndex(utl::narrow_cast<u8>(itr->second));
}

assembly::MemoryAddress RegisterDescriptor::resolveAddr(ir::Value const& address) {
    SC_ASSERT(address.type()->category() == ir::Type::Pointer,
              "address must be a pointer");
    auto const regIdx = std::get<assembly::RegisterIndex>(resolve(address)).index;
    return assembly::MemoryAddress(regIdx, 0, 0);
}

assembly::RegisterIndex RegisterDescriptor::makeTemporary() {
    return assembly::RegisterIndex(index++);
}

assembly::RegisterIndex RegisterDescriptor::allocateAutomatic(size_t numRegisters) {
    assembly::RegisterIndex const result(utl::narrow_cast<u8>(index));
    index += numRegisters;
    return result;
}
