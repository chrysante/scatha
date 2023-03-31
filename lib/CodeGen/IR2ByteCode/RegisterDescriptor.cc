#include "CodeGen/IR2ByteCode/RegisterDescriptor.h"

#include <utl/utility.hpp>

#include "IR/CFG.h"
#include "IR/Type.h"

using namespace scatha;
using namespace cg;
using namespace Asm;

Value RegisterDescriptor::resolve(ir::Value const& value) {
    // clang-format off
    return visit(value, utl::overload{
        [&](ir::IntegralConstant const& constant) -> Asm::Value {
            switch (constant.type()->size()) {
            case 1:
                return Value8(constant.value().to<u8>());
            case 2:
                return Value16(constant.value().to<u16>());
            case 4:
                return Value32(constant.value().to<u32>());
            case 8:
                return Value64(constant.value().to<u64>());
            default:
                SC_UNREACHABLE();
            }
        },
        [&](ir::FloatingPointConstant const& constant) {
            return Value64(constant.value().to<f64>());
        },
        [&](ir::UndefValue const&) {
            return RegisterIndex(0);
        },
        [&](ir::Value const& value) {
            SC_ASSERT(!value.name().empty(), "Name must not be empty.");
            auto const [itr, success] =
                values.insert({ std::string(value.name()), index });
            if (success) {
                index += utl::ceil_divide(value.type()->size(), 8);
            }
            return RegisterIndex(utl::narrow_cast<u8>(itr->second));
        }
    }); // clang-format on
}

MemoryAddress RegisterDescriptor::resolveAddr(ir::Value const& address) {
    SC_ASSERT(isa<ir::PointerType>(address.type()),
              "address must be a pointer");
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
