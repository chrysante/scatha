#include "VM/OpCode.h"

#include <cassert>
#include <iostream>
#include <ostream>

#include <utl/functional.hpp>
#include <utl/utility.hpp>

#include "Basic/Memory.h"
#include "VM/VirtualMachine.h"

#define VM_ASSERT(COND) assert(COND)
#define VM_WARNING(COND, MSG)                                                                                          \
    do {                                                                                                               \
        if (!(COND)) {                                                                                                 \
            std::cout << (MSG);                                                                                        \
        }                                                                                                              \
    } while (0)

namespace scatha::vm {

std::ostream& operator<<(std::ostream& str, OpCode c) {
    // clang-format off
    return str << UTL_SERIALIZE_ENUM(c, {
        { OpCode::enterFn,         "enterFn" },
        { OpCode::setBrk,          "setBrk" },
        { OpCode::call,            "call" },
        { OpCode::ret,             "ret" },
        { OpCode::terminate,       "terminate" },
        { OpCode::movRR,           "movRR" },
        { OpCode::movRV,           "movRV" },
        { OpCode::movMR,           "movMR" },
        { OpCode::movRM,           "movRM" },
        { OpCode::alloca_,         "alloca" },
        { OpCode::jmp,             "jmp" },
        { OpCode::je,              "je" },
        { OpCode::jne,             "jne" },
        { OpCode::jl,              "jl" },
        { OpCode::jle,             "jle" },
        { OpCode::jg,              "jg" },
        { OpCode::jge,             "jge" },
        { OpCode::ucmpRR,          "ucmpRR" },
        { OpCode::icmpRR,          "icmpRR" },
        { OpCode::ucmpRV,          "ucmpRV" },
        { OpCode::icmpRV,          "icmpRV" },
        { OpCode::fcmpRR,          "fcmpRR" },
        { OpCode::fcmpRV,          "fcmpRV" },
        { OpCode::itest,           "itest" },
        { OpCode::utest,           "utest" },
        { OpCode::sete,            "sete" },
        { OpCode::setne,           "setne" },
        { OpCode::setl,            "setl" },
        { OpCode::setle,           "setle" },
        { OpCode::setg,            "setg" },
        { OpCode::setge,           "setge" },
        { OpCode::lnt,             "lnt" },
        { OpCode::bnt,             "bnt" },
        { OpCode::addRR,           "addRR" },
        { OpCode::addRV,           "addRV" },
        { OpCode::addRM,           "addRM" },
        { OpCode::subRR,           "subRR" },
        { OpCode::subRV,           "subRV" },
        { OpCode::subRM,           "subRM" },
        { OpCode::mulRR,           "mulRR" },
        { OpCode::mulRV,           "mulRV" },
        { OpCode::mulRM,           "mulRM" },
        { OpCode::divRR,           "divRR" },
        { OpCode::divRV,           "divRV" },
        { OpCode::divRM,           "divRM" },
        { OpCode::idivRR,          "idivRR" },
        { OpCode::idivRV,          "idivRV" },
        { OpCode::idivRM,          "idivRM" },
        { OpCode::remRR,           "remRR" },
        { OpCode::remRV,           "remRV" },
        { OpCode::remRM,           "remRM" },
        { OpCode::iremRR,          "iremRR" },
        { OpCode::iremRV,          "iremRV" },
        { OpCode::iremRM,          "iremRM" },
        { OpCode::faddRR,          "faddRR" },
        { OpCode::faddRV,          "faddRV" },
        { OpCode::faddRM,          "faddRM" },
        { OpCode::fsubRR,          "fsubRR" },
        { OpCode::fsubRV,          "fsubRV" },
        { OpCode::fsubRM,          "fsubRM" },
        { OpCode::fmulRR,          "fmulRR" },
        { OpCode::fmulRV,          "fmulRV" },
        { OpCode::fmulRM,          "fmulRM" },
        { OpCode::fdivRR,          "fdivRR" },
        { OpCode::fdivRV,          "fdivRV" },
        { OpCode::fdivRM,          "fdivRM" },
        { OpCode::slRR,            "slRR" },
        { OpCode::slRV,            "slRV" },
        { OpCode::srRR,            "srRR" },
        { OpCode::srRV,            "srRV" },
        { OpCode::andRR,           "andRR" },
        { OpCode::andRV,           "andRV" },
        { OpCode::orRR,            "orRR" },
        { OpCode::orRV,            "orRV" },
        { OpCode::xorRR,           "xorRR" },
        { OpCode::xorRV,           "xorRV" },
        { OpCode::callExt,         "callExt" },
    });
    // clang-format on
}

struct OpCodeImpl {

    static u8* getPointer(u64 const* reg, u8 const* i) {
        const size_t ptrRegIdx = i[0];
        const ssize_t offset    = i[1];
        int const offsetShift  = i[2];
        return reinterpret_cast<u8*>(reg[ptrRegIdx]) + (offset << offsetShift);
    }

    template <OpCode C>
    static auto jump(auto cond) {
        return [](u8 const* i, u64*, VirtualMachine* vm) -> u64 {
            i32 const offset = read<i32>(&i[0]);
            if (decltype(cond)()(vm->flags)) {
                vm->iptr += offset;
                return 0;
            }
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto compareRR() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            size_t const regIdxB = i[1];
            T const a            = read<T>(&reg[regIdxA]);
            T const b            = read<T>(&reg[regIdxB]);
            vm->flags.less       = a < b;
            vm->flags.equal      = a == b;
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto compareRV() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            T const a            = read<T>(&reg[regIdxA]);
            T const b            = read<T>(i + 1);
            vm->flags.less       = a < b;
            vm->flags.equal      = a == b;
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto testR() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            T const a           = read<T>(&reg[regIdx]);
            vm->flags.less      = a < 0;
            vm->flags.equal     = a == 0;
            return codeSize(C);
        };
    }

    template <OpCode C>
    static auto set(auto setter) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            store(&reg[regIdx], static_cast<u64>(decltype(setter)()(vm->flags)));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto unaryR(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = read<T>(&reg[regIdx]);
            store(&reg[regIdx], decltype(operation)()(a));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRR(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdxA = i[0];
            size_t const regIdxB = i[1];
            auto const a         = read<T>(&reg[regIdxA]);
            auto const b         = read<T>(&reg[regIdxB]);
            store(&reg[regIdxA], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRV(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = read<T>(&reg[regIdx]);
            auto const b        = read<T>(i + 1);
            store(&reg[regIdx], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRM(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            u8* const ptr     = getPointer(reg, i + 1);
            VM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
            auto const a = read<T>(&reg[regIdxA]);
            auto const b = read<T>(ptr);
            store(&reg[regIdxA], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    static utl::vector<Instruction> makeInstructionTable() {
        utl::vector<Instruction> result(static_cast<size_t>(OpCode::_count));
        auto at = [&, idx = 0 ](OpCode i) mutable -> auto& {
            SC_ASSERT(static_cast<int>(i) == idx++, "Missing instruction");
            return result[static_cast<u8>(i)];
        };
        using enum OpCode;

        /// MARK: Register allocation
        at(enterFn) = [](u8 const* i, u64* regPtr, VirtualMachine* vm) -> u64 {
            return codeSize(enterFn);
        };

        /// MARK: Memory allocation
        at(setBrk) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const sizeRegIdx = i[0];
            u64 const size          = reg[sizeRegIdx];
            vm->resizeMemory(size);
            reg[sizeRegIdx] = static_cast<u64>(vm->memoryPtr - vm->memory.data());
            return codeSize(setBrk);
        };

        /// MARK: Function call and return
        at(call) = [](u8 const* i, u64*, VirtualMachine* vm) -> u64 {
            i32 const offset       = read<i32>(i);
            size_t const regOffset = i[4];
            vm->regPtr += regOffset;
            vm->regPtr[-2] = regOffset;
            vm->regPtr[-1] = utl::bit_cast<u64>(vm->iptr + codeSize(call));
            vm->iptr += offset;
            return 0;
        };

        at(ret) = [](u8 const*, u64* regPtr, VirtualMachine* vm) -> u64 {
            if (vm->registers.data() == regPtr) {
                /// Meaning we are the root of the call tree aka. the main function,
                /// so we set the instruction pointer to the program break to terminate execution.
                vm->iptr = vm->programBreak;
                return 0;
            }
            vm->iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
            vm->regPtr -= regPtr[-2];
            return 0;
        };

        at(terminate) = [](u8 const*, u64*, VirtualMachine* vm) -> u64 {
            vm->iptr = vm->programBreak;
            return 0;
        };

        /// MARK: Loads and stores
        at(movRR) = [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const toRegIdx   = i[0];
            size_t const fromRegIdx = i[1];
            reg[toRegIdx]           = reg[fromRegIdx];
            return codeSize(movRR);
        };
        at(movRV) = [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const toRegIdx = i[0];
            reg[toRegIdx]         = read<u64>(i + 1);
            return codeSize(movRV);
        };
        at(movMR) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            u8* const ptr        = getPointer(reg, i);
            size_t const fromRegIdx = i[3];
            VM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
//            VM_ASSERT(ptr >= vm->programBreak && "Trying to write to the instruction set");
            std::memcpy(ptr, &reg[fromRegIdx], 8);
            return codeSize(movMR);
        };
        at(movRM) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const toRegIdx = i[0];
            u8* const ptr      = getPointer(reg, i + 1);
            VM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
//            VM_WARNING(ptr >= vm->programBreak, "Reading memory from the instruction set");
            std::memcpy(&reg[toRegIdx], ptr, 8);
            return codeSize(movRM);
        };
        at(alloca_) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const targetRegIdx = i[0];
            size_t const sourceRegIdx = i[1];
            reg[targetRegIdx] = reinterpret_cast<u64>(&reg[sourceRegIdx]);
            return codeSize(alloca_);
        };
        
        /// MARK: Jumps
        at(jmp) = jump<jmp>([](VMFlags) { return true; });
        at(je)  = jump<je>([](VMFlags f) { return f.equal; });
        at(jne) = jump<jne>([](VMFlags f) { return !f.equal; });
        at(jl)  = jump<jl>([](VMFlags f) { return f.less; });
        at(jle) = jump<jle>([](VMFlags f) { return f.less || f.equal; });
        at(jg)  = jump<jg>([](VMFlags f) { return !f.less && !f.equal; });
        at(jge) = jump<jge>([](VMFlags f) { return !f.less; });

        /// MARK: Comparison
        at(ucmpRR) = compareRR<ucmpRR, u64>();
        at(icmpRR) = compareRR<icmpRR, i64>();
        at(ucmpRV) = compareRV<ucmpRV, u64>();
        at(icmpRV) = compareRV<icmpRV, i64>();
        at(fcmpRR) = compareRR<fcmpRR, f64>();
        at(fcmpRV) = compareRV<fcmpRV, f64>();
        at(itest)  = testR<itest, i64>();
        at(utest)  = testR<utest, u64>();

        /// MARK: Read comparison results
        at(sete)  = set<sete>([](VMFlags f) { return f.equal; });
        at(setne) = set<setne>([](VMFlags f) { return !f.equal; });
        at(setl)  = set<setl>([](VMFlags f) { return f.less; });
        at(setle) = set<setle>([](VMFlags f) { return f.less || f.equal; });
        at(setg)  = set<setg>([](VMFlags f) { return !f.less && !f.equal; });
        at(setge) = set<setge>([](VMFlags f) { return !f.less; });

        /// MARK: Unary operations
        at(lnt) = unaryR<lnt, u64>(utl::logical_not);
        at(bnt) = unaryR<bnt, u64>(utl::bitwise_not);

        /// MARK: Integer arithmetic
        at(addRR)  = arithmeticRR<addRR, u64>(utl::plus);
        at(addRV)  = arithmeticRV<addRV, u64>(utl::plus);
        at(addRM)  = arithmeticRM<addRM, u64>(utl::plus);
        at(subRR)  = arithmeticRR<subRR, u64>(utl::minus);
        at(subRV)  = arithmeticRV<subRV, u64>(utl::minus);
        at(subRM)  = arithmeticRM<subRM, u64>(utl::minus);
        at(mulRR)  = arithmeticRR<mulRR, u64>(utl::multiplies);
        at(mulRV)  = arithmeticRV<mulRV, u64>(utl::multiplies);
        at(mulRM)  = arithmeticRM<mulRM, u64>(utl::multiplies);
        at(divRR)  = arithmeticRR<divRR, u64>(utl::divides);
        at(divRV)  = arithmeticRV<divRV, u64>(utl::divides);
        at(divRM)  = arithmeticRM<divRM, u64>(utl::divides);
        at(idivRR) = arithmeticRR<idivRR, i64>(utl::divides);
        at(idivRV) = arithmeticRV<idivRV, i64>(utl::divides);
        at(idivRM) = arithmeticRM<idivRM, i64>(utl::divides);
        at(remRR)  = arithmeticRR<remRR, u64>(utl::modulo);
        at(remRV)  = arithmeticRV<remRV, u64>(utl::modulo);
        at(remRM)  = arithmeticRM<remRM, u64>(utl::modulo);
        at(iremRR) = arithmeticRR<iremRR, i64>(utl::modulo);
        at(iremRV) = arithmeticRV<iremRV, i64>(utl::modulo);
        at(iremRM) = arithmeticRM<iremRM, i64>(utl::modulo);

        /// MARK: Floating point arithmetic
        at(faddRR) = arithmeticRR<faddRR, f64>(utl::plus);
        at(faddRV) = arithmeticRV<faddRV, f64>(utl::plus);
        at(faddRM) = arithmeticRM<faddRM, f64>(utl::plus);
        at(fsubRR) = arithmeticRR<fsubRR, f64>(utl::minus);
        at(fsubRV) = arithmeticRV<fsubRV, f64>(utl::minus);
        at(fsubRM) = arithmeticRM<fsubRM, f64>(utl::minus);
        at(fmulRR) = arithmeticRR<fmulRR, f64>(utl::multiplies);
        at(fmulRV) = arithmeticRV<fmulRV, f64>(utl::multiplies);
        at(fmulRM) = arithmeticRM<fmulRM, f64>(utl::multiplies);
        at(fdivRR) = arithmeticRR<fdivRR, f64>(utl::divides);
        at(fdivRV) = arithmeticRV<fdivRV, f64>(utl::divides);
        at(fdivRM) = arithmeticRM<fdivRM, f64>(utl::divides);

        at(slRR) = arithmeticRR<slRR, u64>(utl::leftshift);
        at(slRV) = arithmeticRV<slRV, u64>(utl::leftshift);
        at(srRR) = arithmeticRR<srRR, u64>(utl::rightshift);
        at(srRV) = arithmeticRV<srRV, u64>(utl::rightshift);

        at(andRR) = arithmeticRR<andRR, u64>(utl::bitwise_and);
        at(andRV) = arithmeticRV<andRV, u64>(utl::bitwise_and);
        at(orRR)  = arithmeticRR<orRR, u64>(utl::bitwise_or);
        at(orRV)  = arithmeticRV<orRV, u64>(utl::bitwise_or);
        at(xorRR) = arithmeticRR<orRR, u64>(utl::bitwise_xor);
        at(xorRV) = arithmeticRV<orRV, u64>(utl::bitwise_xor);

        /// MARK: Misc
        at(callExt) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx       = i[0];
            size_t const tableIdx     = i[1];
            size_t const idxIntoTable = read<u16>(&i[2]);
            vm->extFunctionTable[tableIdx][idxIntoTable](reg[regIdx], vm);
            return codeSize(callExt);
        };

        return result;
    }
};

utl::vector<Instruction> makeInstructionTable() {
    return OpCodeImpl::makeInstructionTable();
}

} // namespace scatha::vm
