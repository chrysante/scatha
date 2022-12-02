#ifndef SCATHA_VM_INSTRUCTION_H_
#define SCATHA_VM_INSTRUCTION_H_

#include <iosfwd>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {

/*

 A program looks like this:
 u8 [instruction], u8... [arguments]
 ...

 MEMORY_POINTER           ==   u8 ptrRegIdx, u8 offset, u8 offsetShift
 eval(MEMORY_POINTER)     ==   reg[ptrRegIdx] + (offset << offsetShift)
 sizeof(MEMORY_POINTER)   ==   3

 */

/** MARK: Calling convention
 * All register indices are from the position of the callee.
 *
 *  Arguments are passed in consecutive registers starting with index 0.
 *  Return value is passed in consecutive registers starting with index 0.
 *  All registers with positive indices may be used and modified by the callee.
 *  Registers to hold the arguments are allocated by the caller, all further
 * registers must be allocted by the callee (using \p allocReg). The register
 * pointer offset is placed in \p R[-2] and added to the register pointer by the
 * \p call instruction. The register pointer offset is subtracted from the
 * register pointer by the the \p ret instruction. The return address is placed
 * in \p R[-1] by the \p call instruction.
 */

enum class OpCode : u8 {
    /// MARK: Register allocation
    /// After executing \p allocReg all registers with index less than \p
    /// numRegisters will be available.
    /// This means for a called function, the amount of available registers will
    /// include the argument registers set by the caller.
    allocReg, // (u8 numRegisters)

    /// MARK: Memory allocation
    /// Places a pointer to beginning of memory section in the argument
    /// register.
    setBrk, // (u8 sizeRegIdx)

    /// MARK: Function call and return
    /// Performs the following operations:
    // regPtr += regOffset
    // regPtr[-2] = regOffset
    // regPtr[-1] = iptr
    // jmp offset
    call, // (i32 offset, u8 regOffset)

    /// Performs the following operations:
    // iptr = regPtr[-1]
    // regPtr -= regPtr[-2]
    ret, // ()

    /// Immediately terminates the program.
    terminate, // ()

    /// MARK: Loads and stores
    /// Copies the value from the source operand (right) into the destination
    /// operand (right).
    movRR, // (u8 toRegIdx, u8 fromRegIdx)
    movRV, // (u8 toRegIdx, u64 value)
    movMR, // (MEMORY_POINTER, u8 ptrRegIdx)
    movRM, // (u8 ptrRegIdx, MEMORY_POINTER)

    /// MARK: Jumps
    /// Jumps are performed by adding the \p offset argument the the instruction
    /// pointer.
    /// This means a jump with \p offset 0 will jump to itself and thus enter an
    /// infinite loop.

    /// Jump to the specified offset.
    jmp, // (i32 offset)
    /// Jump to the specified offset if equal flag is set.
    je, // (i32 offset)
    /// Jump to the specified offset if equal flag is not set.
    jne, // (i32 offset)
    /// Jump to the specified offset if less flag is set.
    jl, // (i32 offset)
    /// Jump to the specified offset if less flag or equal flag is set.
    jle, // (i32 offset)
    /// Jump to the specified offset if less flag and equal flag are not set.
    jg, // (i32 offset)
    /// Jump to the specified offset if less flag is not set.
    jge, // (i32 offset)

    /// MARK: Comparison
    /// Compare the operands and set flags accordingly
    ucmpRR, //  (u8 regIdxA, u8 regIdxB)
    icmpRR, //  (u8 regIdxA, u8 regIdxB)
    ucmpRV, //  (u8 regIdxA, u64 value)
    icmpRV, //  (u8 regIdxA, u64 value)
    fcmpRR, //  (u8 regIdxA, u8 regIdxB)
    fcmpRV, //  (u8 regIdxA, u64 value)

    /// Compare the operand to 0 and set flags accordingly
    itest, //  (u8 regIdx)
    utest, //  (u8 regIdx)

    /// MARK: Read comparison results
    /// Set register to 0 or 1 based of the flags set by the *cmp* or *test
    /// instructions
    sete,  //  (u8 regIdx)
    setne, //  (u8 regIdx)
    setl,  //  (u8 regIdx)
    setle, //  (u8 regIdx)
    setg,  //  (u8 regIdx)
    setge, //  (u8 regIdx)

    /// MARK: Unary operations
    // reg[regIdx] = !reg[regIdx]
    lnt, // (u8 regIdx)
    // reg[regIdx] = ~reg[regIdx]
    bnt, // (u8 regIdx)

    /// MARK: Integer arithmetic
    // reg[regIdxA] += reg[regIdxA]
    addRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] += value
    addRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] += memory[eval(MEMORY_POINTER)]
    addRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] -= reg[regIdxA]
    subRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] -= value
    subRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
    subRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] *= reg[regIdxA]
    mulRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] *= value
    mulRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
    mulRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] /= reg[regIdxA]
    divRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] /= value
    divRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
    divRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] /= reg[regIdxA]
    idivRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] /= value (signed arithmetic)
    idivRV, // (u8 regIdx, u64 value) (signed arithmetic)
    // reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
    idivRM, // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)
    // reg[regIdxA] %= reg[regIdxA]
    remRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] %= value
    remRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
    remRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] %= reg[regIdxA]
    iremRR, // (u8 regIdxA, u8 regIdxA) (signed arithmetic)
    // reg[regIdx] %= value
    iremRV, // (u8 regIdx, u64 value) (signed arithmetic)
    // reg[regIdxA] %= memory[eval(MEMORY_POINTER)]
    iremRM, // (u8 regIdxA, MEMORY_POINTER) (signed arithmetic)

    /// MARK: Floating point arithmetic
    // reg[regIdxA] += reg[regIdxA]
    faddRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] += value
    faddRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] += memory[eval(MEMORY_POINTER)]
    faddRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] -= reg[regIdxA]
    fsubRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] -= value
    fsubRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] -= memory[eval(MEMORY_POINTER)]
    fsubRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] *= reg[regIdxA]
    fmulRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx] *= value
    fmulRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] *= memory[eval(MEMORY_POINTER)]
    fmulRM, // (u8 regIdxA, MEMORY_POINTER)
    // reg[regIdxA] /= reg[regIdxA]
    fdivRR, // (u8 regIdxA, u8 regIdxA)
    // reg[regIdx]  /= value
    fdivRV, // (u8 regIdx, u64 value)
    // reg[regIdxA] /= memory[eval(MEMORY_POINTER)]
    fdivRM, // (u8 regIdxA, MEMORY_POINTER)

    /// MARK: Bitshift
    slRR, //  (u8 regIdxA, u8 regIdxB)
    slRV, //  (u8 regIdxA, u64 value)
    srRR, //  (u8 regIdxA, u8 regIdxB)
    srRV, //  (u8 regIdxA, u64 value)

    /// MARK: Bitwise AND/OR
    andRR, //  (u8 regIdxA, u8 regIdxB)
    andRV, //  (u8 regIdxA, u64 value)
    orRR,  //  (u8 regIdxA, u8 regIdxB)
    orRV,  //  (u8 regIdxA, u64 value)
    xorRR, //  (u8 regIdxA, u8 regIdxB)
    xorRV, //  (u8 regIdxA, u64 value)

    /// MARK: Misc
    // extFunctionTable[tableIdx][idxIntoTable](reg[regIdx], this)
    callExt, // (u8 regIdx, u8 tableIdx, u16 idxIntoTable)

    _count
};

std::ostream& operator<<(std::ostream&, OpCode);

enum class OpCodeClass { RR, RV, RM, MR, R, Jump, Other, _count };

constexpr bool isJump(OpCode c) {
    using enum OpCode;
    u8 const cRawValue = static_cast<u8>(c);
    return (cRawValue >= static_cast<u8>(jmp) && cRawValue <= static_cast<u8>(jge)) || c == call;
}

constexpr OpCodeClass classify(OpCode c) {
    return UTL_MAP_ENUM(c,
                        OpCodeClass,
                        {
                            { OpCode::allocReg, OpCodeClass::Other },  { OpCode::setBrk, OpCodeClass::Other },
                            { OpCode::call, OpCodeClass::Other },      { OpCode::ret, OpCodeClass::Other },
                            { OpCode::terminate, OpCodeClass::Other }, { OpCode::movRR, OpCodeClass::RR },
                            { OpCode::movRV, OpCodeClass::RV },        { OpCode::movMR, OpCodeClass::MR },
                            { OpCode::movRM, OpCodeClass::RM },        { OpCode::jmp, OpCodeClass::Jump },
                            { OpCode::je, OpCodeClass::Jump },         { OpCode::jne, OpCodeClass::Jump },
                            { OpCode::jl, OpCodeClass::Jump },         { OpCode::jle, OpCodeClass::Jump },
                            { OpCode::jg, OpCodeClass::Jump },         { OpCode::jge, OpCodeClass::Jump },
                            { OpCode::ucmpRR, OpCodeClass::RR },       { OpCode::icmpRR, OpCodeClass::RR },
                            { OpCode::ucmpRV, OpCodeClass::RV },       { OpCode::icmpRV, OpCodeClass::RV },
                            { OpCode::fcmpRR, OpCodeClass::RR },       { OpCode::fcmpRV, OpCodeClass::RV },
                            { OpCode::itest, OpCodeClass::R },         { OpCode::utest, OpCodeClass::R },
                            { OpCode::sete, OpCodeClass::R },          { OpCode::setne, OpCodeClass::R },
                            { OpCode::setl, OpCodeClass::R },          { OpCode::setle, OpCodeClass::R },
                            { OpCode::setg, OpCodeClass::R },          { OpCode::setge, OpCodeClass::R },
                            { OpCode::lnt, OpCodeClass::R },           { OpCode::bnt, OpCodeClass::R },
                            { OpCode::addRR, OpCodeClass::RR },        { OpCode::addRV, OpCodeClass::RV },
                            { OpCode::addRM, OpCodeClass::RM },        { OpCode::subRR, OpCodeClass::RR },
                            { OpCode::subRV, OpCodeClass::RV },        { OpCode::subRM, OpCodeClass::RM },
                            { OpCode::mulRR, OpCodeClass::RR },        { OpCode::mulRV, OpCodeClass::RV },
                            { OpCode::mulRM, OpCodeClass::RM },        { OpCode::divRR, OpCodeClass::RR },
                            { OpCode::divRV, OpCodeClass::RV },        { OpCode::divRM, OpCodeClass::RM },
                            { OpCode::idivRR, OpCodeClass::RR },       { OpCode::idivRV, OpCodeClass::RV },
                            { OpCode::idivRM, OpCodeClass::RM },       { OpCode::remRR, OpCodeClass::RR },
                            { OpCode::remRV, OpCodeClass::RV },        { OpCode::remRM, OpCodeClass::RM },
                            { OpCode::iremRR, OpCodeClass::RR },       { OpCode::iremRV, OpCodeClass::RV },
                            { OpCode::iremRM, OpCodeClass::RM },       { OpCode::faddRR, OpCodeClass::RR },
                            { OpCode::faddRV, OpCodeClass::RV },       { OpCode::faddRM, OpCodeClass::RM },
                            { OpCode::fsubRR, OpCodeClass::RR },       { OpCode::fsubRV, OpCodeClass::RV },
                            { OpCode::fsubRM, OpCodeClass::RM },       { OpCode::fmulRR, OpCodeClass::RR },
                            { OpCode::fmulRV, OpCodeClass::RV },       { OpCode::fmulRM, OpCodeClass::RM },
                            { OpCode::fdivRR, OpCodeClass::RR },       { OpCode::fdivRV, OpCodeClass::RV },
                            { OpCode::fdivRM, OpCodeClass::RM },       { OpCode::slRR, OpCodeClass::RR },
                            { OpCode::slRV, OpCodeClass::RV },         { OpCode::srRR, OpCodeClass::RR },
                            { OpCode::srRV, OpCodeClass::RV },         { OpCode::andRR, OpCodeClass::RR },
                            { OpCode::andRV, OpCodeClass::RV },        { OpCode::orRR, OpCodeClass::RR },
                            { OpCode::orRV, OpCodeClass::RV },         { OpCode::xorRR, OpCodeClass::RR },
                            { OpCode::xorRV, OpCodeClass::RV },        { OpCode::callExt, OpCodeClass::Other },
                        });
}

constexpr size_t codeSize(OpCode c) {
    using enum OpCodeClass;
    auto const opCodeClass = classify(c);
    if (opCodeClass == Other) {
        switch (c) {
        case OpCode::allocReg: return 2;
        case OpCode::setBrk: return 2;
        case OpCode::call: return 6;
        case OpCode::ret: return 1;
        case OpCode::terminate: return 1;
        case OpCode::callExt: return 5;
        default: SC_UNREACHABLE();
        }
    }
    return UTL_MAP_ENUM(opCodeClass,
                        size_t,
                        { { OpCodeClass::RR, 3 },
                          { OpCodeClass::RV, 10 },
                          { OpCodeClass::RM, 5 },
                          { OpCodeClass::MR, 5 },
                          { OpCodeClass::R, 2 },
                          { OpCodeClass::Jump, 5 },
                          { OpCodeClass::Other, static_cast<size_t>(-1) } });
}

using Instruction = u64 (*)(u8 const*, u64*, class VirtualMachine*);

utl::vector<Instruction> makeInstructionTable();

} // namespace scatha::vm

#endif // SCATHA_VM_INSTRUCTION_H_
