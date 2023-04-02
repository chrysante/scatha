#include <svm/OpCode.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <ostream>
#include <type_traits>

#include <utl/functional.hpp>
#include <utl/utility.hpp>

#include <svm/VirtualMachine.h>

#include "Memory.h"

#define SVM_ASSERT(COND) assert(COND)
#define SVM_WARNING(COND, MSG)                                                 \
    do {                                                                       \
        if (!(COND)) {                                                         \
            std::cout << (MSG);                                                \
        }                                                                      \
    } while (0)

using namespace svm;

std::string_view svm::toString(OpCode c) {
    return std::array{
#define SVM_INSTRUCTION_DEF(inst, class) std::string_view(#inst),
#include <svm/Lists.def>
    }[static_cast<size_t>(c)];
}

std::ostream& svm::operator<<(std::ostream& str, OpCode c) {
    return str << toString(c);
}

struct svm::OpCodeImpl {

    static u8* getPointer(u64 const* reg, u8 const* i) {
        size_t const baseptrRegIdx         = i[0];
        size_t const offsetCountRegIdx     = i[1];
        i64 const constantOffsetMultiplier = i[2];
        i64 const constantInnerOffset      = i[3];
        u8* const offsetBaseptr =
            utl::bit_cast<u8*>(reg[baseptrRegIdx]) + constantInnerOffset;
        /// See documentation in "OpCode.h"
        if (offsetCountRegIdx == 0xFF) {
            return offsetBaseptr;
        }
        i64 const offsetCount = static_cast<i64>(reg[offsetCountRegIdx]);
        return offsetBaseptr + offsetCount * constantOffsetMultiplier;
    }

    template <OpCode C>
    static auto jump(auto cond) {
        return [](u8 const* i, u64*, VirtualMachine* vm) -> u64 {
            i32 const offset = load<i32>(&i[0]);
            if (decltype(cond)()(vm->flags)) {
                vm->iptr += offset;
                return 0;
            }
            return codeSize(C);
        };
    }

    template <OpCode C, size_t Size>
    static auto moveMR() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            u8* const ptr = getPointer(reg, i);
            SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
            size_t const sourceRegIdx = i[4];
            std::memcpy(ptr, &reg[sourceRegIdx], Size);
            return codeSize(C);
        };
    }

    template <OpCode C, size_t Size>
    static auto moveRM() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx = i[0];
            u8* const ptr           = getPointer(reg, i + 1);
            SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
            reg[destRegIdx] = 0;
            std::memcpy(&reg[destRegIdx], ptr, Size);
            return codeSize(C);
        };
    }

    template <OpCode C>
    static auto condMove64RR(auto cond) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx   = i[0];
            size_t const sourceRegIdx = i[1];
            if (decltype(cond)()(vm->flags)) {
                reg[destRegIdx] = reg[sourceRegIdx];
            }
            return codeSize(C);
        };
    }

    template <OpCode C>
    static auto condMove64RV(auto cond) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx = i[0];
            if (decltype(cond)()(vm->flags)) {
                reg[destRegIdx] = load<u64>(i + 1);
            }
            return codeSize(C);
        };
    }

    template <OpCode C, size_t Size>
    static auto condMoveRM(auto cond) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx = i[0];
            u8* const ptr           = getPointer(reg, i + 1);
            SVM_ASSERT(reinterpret_cast<size_t>(ptr) % Size == 0);
            if (decltype(cond)()(vm->flags)) {
                reg[destRegIdx] = 0;
                std::memcpy(&reg[destRegIdx], ptr, Size);
            }
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto compareRR() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            size_t const regIdxB = i[1];
            T const a            = load<T>(&reg[regIdxA]);
            T const b            = load<T>(&reg[regIdxB]);
            vm->flags.less       = a < b;
            vm->flags.equal      = a == b;
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto compareRV() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            T const a            = load<T>(&reg[regIdxA]);
            T const b            = load<T>(i + 1);
            vm->flags.less       = a < b;
            vm->flags.equal      = a == b;
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto testR() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            T const a           = load<T>(&reg[regIdx]);
            vm->flags.less      = a < 0;
            vm->flags.equal     = a == 0;
            return codeSize(C);
        };
    }

    template <OpCode C>
    static auto set(auto setter) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            store(&reg[regIdx],
                  static_cast<u64>(decltype(setter)()(vm->flags)));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto unaryR(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = load<T>(&reg[regIdx]);
            store(&reg[regIdx], decltype(operation)()(a));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRR(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdxA = i[0];
            size_t const regIdxB = i[1];
            auto const a         = load<T>(&reg[regIdxA]);
            auto const b         = load<T>(&reg[regIdxB]);
            store(&reg[regIdxA], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRV(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = load<T>(&reg[regIdx]);
            auto const b        = load<T>(i + 1);
            store(&reg[regIdx], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    template <OpCode C, typename T>
    static auto arithmeticRM(auto operation) {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdxA = i[0];
            u8* const ptr        = getPointer(reg, i + 1);
            SVM_ASSERT(reinterpret_cast<size_t>(ptr) % 8 == 0);
            auto const a = load<T>(&reg[regIdxA]);
            auto const b = load<T>(ptr);
            store(&reg[regIdxA], decltype(operation)()(a, b));
            return codeSize(C);
        };
    }

    static auto sext1() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = load<int>(&reg[regIdx]);
            store(&reg[regIdx], a & 1 ? static_cast<u64>(-1ull) : 0);
            return codeSize(OpCode::sext1);
        };
    }

    template <OpCode C, typename T>
    static auto sext() {
        return [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regIdx = i[0];
            auto const a        = load<T>(&reg[regIdx]);
            store(&reg[regIdx], static_cast<i64>(a));
            return codeSize(C);
        };
    }

    static utl::vector<Instruction> makeInstructionTable() {
        utl::vector<Instruction> result(static_cast<size_t>(OpCode::_count));
        auto at = [&, idx = 0](OpCode i) mutable -> auto& {
            assert(static_cast<int>(i) == idx++ && "Missing instruction");
            return result[static_cast<u8>(i)];
        };
        using enum OpCode;

        /// ** Function call and return **
        at(call) = [](u8 const* i, u64*, VirtualMachine* vm) -> u64 {
            i32 const offset       = load<i32>(i);
            size_t const regOffset = i[4];
            vm->regPtr += regOffset;
            vm->regPtr[-3] = utl::bit_cast<u64>(vm->stackPtr);
            vm->regPtr[-2] = regOffset;
            vm->regPtr[-1] = utl::bit_cast<u64>(vm->iptr + codeSize(call));
            vm->iptr += offset;
            return 0;
        };

        at(ret) = [](u8 const*, u64* regPtr, VirtualMachine* vm) -> u64 {
            if (vm->registers.data() == regPtr) {
                /// Meaning we are the root of the call tree aka. the main/start
                /// function, so we set the instruction pointer to the program
                /// break to terminate execution.
                vm->iptr = vm->programBreak;
                return 0;
            }
            vm->iptr = utl::bit_cast<u8 const*>(regPtr[-1]);
            vm->regPtr -= regPtr[-2];
            vm->stackPtr = utl::bit_cast<u8*>(regPtr[-3]);
            return 0;
        };

        at(terminate) = [](u8 const*, u64*, VirtualMachine* vm) -> u64 {
            vm->iptr = vm->programBreak;
            return 0;
        };

        /// ** Loads and stores **
        at(mov64RR) = [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const destRegIdx   = i[0];
            size_t const sourceRegIdx = i[1];
            reg[destRegIdx]           = reg[sourceRegIdx];
            return codeSize(mov64RR);
        };
        at(mov64RV) = [](u8 const* i, u64* reg, VirtualMachine*) -> u64 {
            size_t const destRegIdx = i[0];
            reg[destRegIdx]         = load<u64>(i + 1);
            return codeSize(mov64RV);
        };
        at(mov8MR)  = moveMR<mov8MR, 1>();
        at(mov16MR) = moveMR<mov16MR, 2>();
        at(mov32MR) = moveMR<mov32MR, 4>();
        at(mov64MR) = moveMR<mov64MR, 8>();
        at(mov8RM)  = moveRM<mov8RM, 1>();
        at(mov16RM) = moveRM<mov16RM, 2>();
        at(mov32RM) = moveRM<mov32RM, 4>();
        at(mov64RM) = moveRM<mov64RM, 8>();

        /// ** Condition callbacks **
        auto equal     = [](VMFlags f) { return f.equal; };
        auto notEqual  = [](VMFlags f) { return !f.equal; };
        auto less      = [](VMFlags f) { return f.less; };
        auto lessEq    = [](VMFlags f) { return f.less || f.equal; };
        auto greater   = [](VMFlags f) { return !f.less && !f.equal; };
        auto greaterEq = [](VMFlags f) { return !f.less; };

        /// ** Conditional moves **
        at(cmove64RR) = condMove64RR<cmove64RR>(equal);
        at(cmove64RV) = condMove64RV<cmove64RV>(equal);
        at(cmove8RM)  = condMoveRM<cmove8RM, 1>(equal);
        at(cmove16RM) = condMoveRM<cmove16RM, 2>(equal);
        at(cmove32RM) = condMoveRM<cmove32RM, 4>(equal);
        at(cmove64RM) = condMoveRM<cmove64RM, 8>(equal);

        at(cmovne64RR) = condMove64RR<cmovne64RR>(notEqual);
        at(cmovne64RV) = condMove64RV<cmovne64RV>(notEqual);
        at(cmovne8RM)  = condMoveRM<cmovne8RM, 1>(notEqual);
        at(cmovne16RM) = condMoveRM<cmovne16RM, 2>(notEqual);
        at(cmovne32RM) = condMoveRM<cmovne32RM, 4>(notEqual);
        at(cmovne64RM) = condMoveRM<cmovne64RM, 8>(notEqual);

        at(cmovl64RR) = condMove64RR<cmovl64RR>(less);
        at(cmovl64RV) = condMove64RV<cmovl64RV>(less);
        at(cmovl8RM)  = condMoveRM<cmovl8RM, 1>(less);
        at(cmovl16RM) = condMoveRM<cmovl16RM, 2>(less);
        at(cmovl32RM) = condMoveRM<cmovl32RM, 4>(less);
        at(cmovl64RM) = condMoveRM<cmovl64RM, 8>(less);

        at(cmovle64RR) = condMove64RR<cmovle64RR>(lessEq);
        at(cmovle64RV) = condMove64RV<cmovle64RV>(lessEq);
        at(cmovle8RM)  = condMoveRM<cmovle8RM, 1>(lessEq);
        at(cmovle16RM) = condMoveRM<cmovle16RM, 2>(lessEq);
        at(cmovle32RM) = condMoveRM<cmovle32RM, 4>(lessEq);
        at(cmovle64RM) = condMoveRM<cmovle64RM, 8>(lessEq);

        at(cmovg64RR) = condMove64RR<cmovg64RR>(greater);
        at(cmovg64RV) = condMove64RV<cmovg64RV>(greater);
        at(cmovg8RM)  = condMoveRM<cmovg8RM, 1>(greater);
        at(cmovg16RM) = condMoveRM<cmovg16RM, 2>(greater);
        at(cmovg32RM) = condMoveRM<cmovg32RM, 4>(greater);
        at(cmovg64RM) = condMoveRM<cmovg64RM, 8>(greater);

        at(cmovge64RR) = condMove64RR<cmovge64RR>(greaterEq);
        at(cmovge64RV) = condMove64RV<cmovge64RV>(greaterEq);
        at(cmovge8RM)  = condMoveRM<cmovge8RM, 1>(greaterEq);
        at(cmovge16RM) = condMoveRM<cmovge16RM, 2>(greaterEq);
        at(cmovge32RM) = condMoveRM<cmovge32RM, 4>(greaterEq);
        at(cmovge64RM) = condMoveRM<cmovge64RM, 8>(greaterEq);

        /// ** Stack pointer manipulation **
        at(lincsp) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx = load<u8>(i);
            size_t const offset     = load<u16>(i + 1);
            reg[destRegIdx]         = utl::bit_cast<u64>(vm->stackPtr);
            vm->stackPtr += offset;
            return codeSize(lincsp);
        };

        /// ** LEA **
        at(lea) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const destRegIdx = load<u8>(i);
            u8* const ptr           = getPointer(reg, i + 1);
            reg[destRegIdx]         = utl::bit_cast<u64>(ptr);
            return codeSize(lea);
        };

        /// ** Jumps **
        at(jmp) = jump<jmp>([](VMFlags) { return true; });
        at(je)  = jump<je>(equal);
        at(jne) = jump<jne>(notEqual);
        at(jl)  = jump<jl>(less);
        at(jle) = jump<jle>(lessEq);
        at(jg)  = jump<jg>(greater);
        at(jge) = jump<jge>(greaterEq);

        /// ** Comparison **
        at(ucmp8RR)  = compareRR<ucmp8RR, u8>();
        at(ucmp16RR) = compareRR<ucmp16RR, u16>();
        at(ucmp32RR) = compareRR<ucmp32RR, u32>();
        at(ucmp64RR) = compareRR<ucmp64RR, u64>();

        at(scmp8RR)  = compareRR<scmp8RR, i8>();
        at(scmp16RR) = compareRR<scmp16RR, i16>();
        at(scmp32RR) = compareRR<scmp32RR, i32>();
        at(scmp64RR) = compareRR<scmp64RR, i64>();

        at(ucmp8RV)  = compareRV<ucmp8RV, u8>();
        at(ucmp16RV) = compareRV<ucmp16RV, u16>();
        at(ucmp32RV) = compareRV<ucmp32RV, u32>();
        at(ucmp64RV) = compareRV<ucmp64RV, u64>();

        at(scmp8RV)  = compareRV<scmp8RV, i8>();
        at(scmp16RV) = compareRV<scmp16RV, i16>();
        at(scmp32RV) = compareRV<scmp32RV, i32>();
        at(scmp64RV) = compareRV<scmp64RV, i64>();

        at(fcmp32RR) = compareRR<fcmp32RR, f32>();
        at(fcmp64RR) = compareRR<fcmp64RR, f64>();

        at(fcmp32RV) = compareRV<fcmp32RV, f32>();
        at(fcmp64RV) = compareRV<fcmp64RV, f64>();

        at(stest8)  = testR<stest8, i8>();
        at(stest16) = testR<stest16, i16>();
        at(stest32) = testR<stest32, i32>();
        at(stest64) = testR<stest64, i64>();

        at(utest8)  = testR<utest8, u8>();
        at(utest16) = testR<utest16, u16>();
        at(utest32) = testR<utest32, u32>();
        at(utest64) = testR<utest64, u64>();

        /// ** load comparison results **
        at(sete)  = set<sete>([](VMFlags f) { return f.equal; });
        at(setne) = set<setne>([](VMFlags f) { return !f.equal; });
        at(setl)  = set<setl>([](VMFlags f) { return f.less; });
        at(setle) = set<setle>([](VMFlags f) { return f.less || f.equal; });
        at(setg)  = set<setg>([](VMFlags f) { return !f.less && !f.equal; });
        at(setge) = set<setge>([](VMFlags f) { return !f.less; });

        /// ** Unary operations **
        at(lnt) = unaryR<lnt, u64>(utl::logical_not);
        at(bnt) = unaryR<bnt, u64>(utl::bitwise_not);

        /// ** Integer arithmetic **
        at(addRR)  = arithmeticRR<addRR, u64>(utl::plus);
        at(addRV)  = arithmeticRV<addRV, u64>(utl::plus);
        at(addRM)  = arithmeticRM<addRM, u64>(utl::plus);
        at(subRR)  = arithmeticRR<subRR, u64>(utl::minus);
        at(subRV)  = arithmeticRV<subRV, u64>(utl::minus);
        at(subRM)  = arithmeticRM<subRM, u64>(utl::minus);
        at(mulRR)  = arithmeticRR<mulRR, u64>(utl::multiplies);
        at(mulRV)  = arithmeticRV<mulRV, u64>(utl::multiplies);
        at(mulRM)  = arithmeticRM<mulRM, u64>(utl::multiplies);
        at(udivRR) = arithmeticRR<udivRR, u64>(utl::divides);
        at(udivRV) = arithmeticRV<udivRV, u64>(utl::divides);
        at(udivRM) = arithmeticRM<udivRM, u64>(utl::divides);
        at(sdivRR) = arithmeticRR<sdivRR, i64>(utl::divides);
        at(sdivRV) = arithmeticRV<sdivRV, i64>(utl::divides);
        at(sdivRM) = arithmeticRM<sdivRM, i64>(utl::divides);
        at(uremRR) = arithmeticRR<uremRR, u64>(utl::modulo);
        at(uremRV) = arithmeticRV<uremRV, u64>(utl::modulo);
        at(uremRM) = arithmeticRM<uremRM, u64>(utl::modulo);
        at(sremRR) = arithmeticRR<sremRR, i64>(utl::modulo);
        at(sremRV) = arithmeticRV<sremRV, i64>(utl::modulo);
        at(sremRM) = arithmeticRM<sremRM, i64>(utl::modulo);

        /// ** Floating point arithmetic **
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

        at(lslRR) = arithmeticRR<lslRR, u64>(utl::leftshift);
        at(lslRV) = arithmeticRV<lslRV, u64>(utl::leftshift);
        at(lslRM) = arithmeticRM<lslRM, u64>(utl::leftshift);
        at(lsrRR) = arithmeticRR<lsrRR, u64>(utl::rightshift);
        at(lsrRV) = arithmeticRV<lsrRV, u64>(utl::rightshift);
        at(lsrRM) = arithmeticRV<lsrRM, u64>(utl::rightshift);

        at(aslRR) = arithmeticRR<aslRR, u64>(utl::arithmetic_leftshift);
        at(aslRV) = arithmeticRV<aslRV, u64>(utl::arithmetic_leftshift);
        at(aslRM) = arithmeticRM<aslRM, u64>(utl::arithmetic_leftshift);
        at(asrRR) = arithmeticRR<asrRR, u64>(utl::arithmetic_rightshift);
        at(asrRV) = arithmeticRV<asrRV, u64>(utl::arithmetic_rightshift);
        at(asrRM) = arithmeticRV<asrRM, u64>(utl::arithmetic_rightshift);

        at(andRR) = arithmeticRR<andRR, u64>(utl::bitwise_and);
        at(andRV) = arithmeticRV<andRV, u64>(utl::bitwise_and);
        at(andRM) = arithmeticRV<andRM, u64>(utl::bitwise_and);
        at(orRR)  = arithmeticRR<orRR, u64>(utl::bitwise_or);
        at(orRV)  = arithmeticRV<orRV, u64>(utl::bitwise_or);
        at(orRM)  = arithmeticRV<orRM, u64>(utl::bitwise_or);
        at(xorRR) = arithmeticRR<xorRR, u64>(utl::bitwise_xor);
        at(xorRV) = arithmeticRV<xorRV, u64>(utl::bitwise_xor);
        at(xorRM) = arithmeticRV<xorRM, u64>(utl::bitwise_xor);

        /// ** Conversion **
        at(sext1)  = OpCodeImpl::sext1();
        at(sext8)  = sext<sext8, i8>();
        at(sext16) = sext<sext16, i16>();
        at(sext32) = sext<sext32, i32>();

        /// ** Misc **
        at(callExt) = [](u8 const* i, u64* reg, VirtualMachine* vm) -> u64 {
            size_t const regPtrOffset = i[0];
            size_t const tableIdx     = i[1];
            size_t const idxIntoTable = load<u16>(&i[2]);
            vm->extFunctionTable[tableIdx][idxIntoTable](reg + regPtrOffset,
                                                         vm);
            return codeSize(callExt);
        };

        return result;
    }
};

utl::vector<Instruction> svm::makeInstructionTable() {
    return OpCodeImpl::makeInstructionTable();
}
