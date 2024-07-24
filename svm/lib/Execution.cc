#include "VirtualMachine.h"

#include <cassert>

#include <utl/functional.hpp>

#include "ArithmeticOps.h"
#include "Common.h"
#include "Memory.h"
#include "OpCode.h"
#include "VMImpl.h"

using namespace svm;

/// \Returns `codeSize(code)`  except for call and terminate instruction for
/// which this function returns 0. This is used to advance the instruction
/// pointer. Since these three instructions alter the instruction pointer we do
/// not want to advance it any further.
/// Jump instructions subtract the codesize from the target because we have
/// conditional jumps and advance the instruction pointer unconditionally
static constexpr size_t execCodeSizeImpl(OpCode code) {
    if (code == OpCode::call) {
        return 0;
    }
    if (code == OpCode::icallr) {
        return 0;
    }
    if (code == OpCode::icallm) {
        return 0;
    }
    if (code == OpCode::ret) {
        return 0;
    }
    if (code == OpCode::terminate) {
        return 0;
    }
    return codeSize(code);
}

/// Making this a template actually cuts execution time in debug builds in half
/// compared to calling `execCodeSizeImpl()` directly.
template <OpCode Code>
static constexpr size_t ExecCodeSize = execCodeSizeImpl(Code);

template <OpCode Code>
static constexpr size_t CodeSize = codeSize(Code);

template <typename T>
static void storeReg(u64* dest, T const& t) {
    std::memset(dest, 0, 8);
    std::memcpy(dest, &t, sizeof(T));
}

static VirtualPointer getPointer(u64 const* reg, u8 const* i) {
    uint64_t baseptrRegIdx = i[0];
    uint64_t offsetCountRegIdx = i[1];
    i64 constantOffsetMultiplier = i[2];
    i64 constantInnerOffset = i[3];
    VirtualPointer offsetBaseptr =
        std::bit_cast<VirtualPointer>(reg[baseptrRegIdx]) + constantInnerOffset;
    /// See documentation in "OpCode.h"
    if (offsetCountRegIdx == 0xFF) {
        return offsetBaseptr;
    }
    i64 offsetCount = static_cast<i64>(reg[offsetCountRegIdx]);
    return offsetBaseptr + offsetCount * constantOffsetMultiplier;
}

#define CHECK_ALIGNED(Kind, ptr, Size)                                         \
    do {                                                                       \
        if (SVM_UNLIKELY(!isAligned(ptr, Size))) {                             \
            throwError<MemoryAccessError>(                                     \
                UTL_CONCAT(MemoryAccessError::Misaligned, Kind), ptr, Size);   \
        }                                                                      \
    } while (0)

template <size_t Size>
static void moveMR(VirtualMemory& memory, u8 const* i, u64* reg) {
    VirtualPointer ptr = getPointer(reg, i);
    CHECK_ALIGNED(Load, ptr, Size);
    size_t const sourceRegIdx = i[4];
    std::memcpy(memory.dereference(ptr, Size), &reg[sourceRegIdx], Size);
}

template <size_t Size>
static void moveRM(VirtualMemory& memory, u8 const* i, u64* reg) {
    size_t const destRegIdx = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    CHECK_ALIGNED(Store, ptr, Size);
    reg[destRegIdx] = 0;
    std::memcpy(&reg[destRegIdx], memory.dereference(ptr, Size), Size);
}

static void condMove64RR(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    size_t const sourceRegIdx = i[1];
    if (cond) {
        reg[destRegIdx] = reg[sourceRegIdx];
    }
}

static void condMove64RV(u8 const* i, u64* reg, bool cond) {
    size_t const destRegIdx = i[0];
    if (cond) {
        reg[destRegIdx] = load<u64>(i + 1);
    }
}

template <size_t Size>
static void condMoveRM(VirtualMemory& memory, u8 const* i, u64* reg,
                       bool cond) {
    size_t const destRegIdx = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    if (cond) {
        CHECK_ALIGNED(Load, ptr, Size);
        reg[destRegIdx] = 0;
        std::memcpy(&reg[destRegIdx], memory.dereference(ptr, Size), Size);
    }
}

template <OpCode C>
static void performCall(VirtualMemory& memory, u8 const* i, u8 const* binary,
                        ExecutionFrame& currentFrame) {
    auto const [dest, regOffset] = [&] {
        if constexpr (C == OpCode::call) {
            /// Yes, unlike in the indirect call cases we load a 32 bit dest
            /// address here
            return std::pair{ load<u32>(i), load<u8>(i + 4) };
        }
        else if constexpr (C == OpCode::icallr) {
            auto* reg = currentFrame.regPtr;
            u8 const idx = load<u8>(i);
            return std::pair{ load<u64>(reg + idx), load<u8>(i + 1) };
        }
        else {
            static_assert(C == OpCode::icallm);
            auto* reg = currentFrame.regPtr;
            VirtualPointer destAddr = getPointer(reg, i);
            return std::pair{ load<u64>(memory.dereference(destAddr, 8)),
                              load<u8>(i + 4) };
        }
    }();
    currentFrame.regPtr += regOffset;
    currentFrame.regPtr[-3] = utl::bit_cast<u64>(currentFrame.stackPtr);
    currentFrame.regPtr[-2] = regOffset;
    auto* retAddr = currentFrame.iptr + CodeSize<C>;
    currentFrame.regPtr[-1] = utl::bit_cast<u64>(retAddr);
    currentFrame.iptr = binary + dest;
}

template <OpCode C>
static void jump(u8 const* i, u8 const* binary, ExecutionFrame& currentFrame,
                 bool cond) {
    u32 dest = load<u32>(&i[0]);
    if (cond) {
        /// `ExecCodeSize` is added to the instruction pointer after executing
        /// any instruction. Because we want the instruction pointer to be
        /// `binary + dest` we subtract that number here.
        currentFrame.iptr = binary + dest - ExecCodeSize<C>;
    }
}

template <typename T>
static void compareRR(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(&reg[regIdxB]);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
static void compareRV(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdxA = i[0];
    T const a = load<T>(&reg[regIdxA]);
    T const b = load<T>(i + 1);
    flags.less = a < b;
    flags.equal = a == b;
}

template <typename T>
static void testR(u8 const* i, u64* reg, CompareFlags& flags) {
    size_t const regIdx = i[0];
    T const a = load<T>(&reg[regIdx]);
    flags.less = a < 0;
    flags.equal = a == 0;
}

static void set(u8 const* i, u64* reg, bool value) {
    size_t const regIdx = i[0];
    storeReg(&reg[regIdx], value);
}

template <typename T>
static void unaryR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<T>(&reg[regIdx]);
    storeReg(&reg[regIdx], decltype(operation)()(a));
}

template <typename T>
static void arithmeticRR(u8 const* i, u64* reg, auto operation) {
    size_t const regIdxA = i[0];
    size_t const regIdxB = i[1];
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(&reg[regIdxB]);
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

template <typename LhsType, typename RhsType = LhsType>
static void arithmeticRV(u8 const* i, u64* reg, auto operation) {
    size_t const regIdx = i[0];
    auto const a = load<LhsType>(&reg[regIdx]);
    auto const b = load<RhsType>(i + 1);
    storeReg(&reg[regIdx], static_cast<LhsType>(decltype(operation)()(a, b)));
}

template <typename T>
static void arithmeticRM(VirtualMemory& memory, u8 const* i, u64* reg,
                         auto operation) {
    size_t const regIdxA = i[0];
    VirtualPointer ptr = getPointer(reg, i + 1);
    CHECK_ALIGNED(Load, ptr, alignof(T));
    auto const a = load<T>(&reg[regIdxA]);
    auto const b = load<T>(memory.dereference(ptr, sizeof(T)));
    storeReg(&reg[regIdxA], decltype(operation)()(a, b));
}

static void sext1(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<int>(&reg[regIdx]);
    storeReg(&reg[regIdx], a & 1 ? static_cast<u64>(~0ull) : 0);
}

template <typename From, typename To>
static void convert(u8 const* i, u64* reg) {
    size_t const regIdx = i[0];
    auto const a = load<From>(&reg[regIdx]);
    storeReg(&reg[regIdx], static_cast<To>(a));
}

/// ## Conditions
static bool equal(CompareFlags f) { return f.equal; }
static bool notEqual(CompareFlags f) { return !f.equal; }
static bool less(CompareFlags f) { return f.less; }
static bool lessEq(CompareFlags f) { return f.less || f.equal; }
static bool greater(CompareFlags f) { return !f.less && !f.equal; }
static bool greaterEq(CompareFlags f) { return !f.less; }

static size_t alignTo(size_t offset, size_t align) {
    if (offset % align == 0) {
        return offset;
    }
    return offset + align - offset % align;
}

namespace {

enum class FIIStructVisitLevel { TopLevel, Nested };

} // namespace

/// Recursively visits structures and dereferences all pointer members
template <FIIStructVisitLevel Level>
static void* dereferenceFFIPtrArg(u8* argPtr, ffi_type const* type,
                                  VirtualMemory& memory) {
    using enum FIIStructVisitLevel;
    auto deref = [&memory](u8* arg) {
        auto* arg64 = reinterpret_cast<u64*>(arg);
        *arg64 = std::bit_cast<u64>(
            memory.nativeToHost(std::bit_cast<VirtualPointer>(*arg64)));
    };
    if (type->type == FFI_TYPE_POINTER) {
        deref(argPtr);
        return argPtr;
    }
    if (type->type == FFI_TYPE_STRUCT) {
        if (Level == TopLevel && type->size > 16) {
            deref(argPtr);
            argPtr = *(u8**)argPtr;
        }
        size_t offset = 0;
        auto** elemPtr = type->elements;
        while (*elemPtr) {
            auto* elem = *elemPtr;
            dereferenceFFIPtrArg<Nested>(argPtr + offset, elem, memory);
            offset = alignTo(offset, elem->alignment) + elem->size;
            ++elemPtr;
        }
        return argPtr;
    }
    return argPtr;
}

static size_t argSizeInWords(ffi_type const* type) {
    if (type->type == FFI_TYPE_STRUCT && type->size > 16) {
        return 1;
    }
    return utl::ceil_divide(type->size, 8);
}

static void invokeFFI(ForeignFunction& F, u64* regPtr, VirtualMemory& memory) {
#ifndef _MSC_VER
    using enum FIIStructVisitLevel;
    u64* argPtr = regPtr;
    u64* retPtr = regPtr;
    if (F.returnType->type == FFI_TYPE_STRUCT && F.returnType->size > 16) {
        ++argPtr;
        auto vretPtr = std::bit_cast<VirtualPointer>(*retPtr);
        retPtr = (u64*)memory.dereference(vretPtr, F.returnType->size);
    }
    for (size_t i = 0; i < F.arguments.size(); ++i) {
        auto* argType = F.argumentTypes[i];
        F.arguments[i] =
            dereferenceFFIPtrArg<TopLevel>(reinterpret_cast<u8*>(argPtr),
                                           argType, memory);
        argPtr += argSizeInWords(argType);
    }
    ffi_call(&F.callInterface, F.funcPtr, retPtr, F.arguments.data());
#else
    throwError<FFIError>(FFIError::FailedToInit, F.name);
#endif
}

/// Computed gotos supported by GCC allow jump threading
#ifdef __GNUC__
#define JUMP_THREADING 1
#endif // __GNUC__

u64 const* VMImpl::execute(size_t start, std::span<u64 const> arguments) {
#if JUMP_THREADING

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu"
#endif

    beginExecution(start, arguments);

    // Jumps directly the the next instruction. We use
    // `std::min(., InvalidOpcodeIndex)` to ensure we never read past the end
    // of the jump table, even with a corrupted binary
#define GOTO_NEXT()                                                            \
    goto* jumpTable[std::min(*currentFrame.iptr, InvalidOpcodeIndex)];

#define TERMINATE_EXECUTION()                                                  \
    do {                                                                       \
        currentFrame.iptr = programBreak;                                      \
        return endExecution();                                                 \
    } while (0)

    void* jumpTable[] = {
#define SVM_INSTRUCTION_DEF(name, ...) &&opcode_block_##name,
#include "OpCode.def.h"
        &&opcode_block_invalid
    };
    static constexpr uint8_t JumpTableSize{ sizeof(jumpTable) / sizeof(void*) };
    static constexpr uint8_t InvalidOpcodeIndex{ JumpTableSize - 1 };

    using enum OpCode;

    // Execution starts by jumping to the first instruction
    GOTO_NEXT()

#define INST(opcode, ...)                                                      \
    opcode_block_##opcode: {                                                   \
        [[maybe_unused]] auto* iptr = currentFrame.iptr + sizeof(OpCode);      \
        [[maybe_unused]] auto* regPtr = currentFrame.regPtr;                   \
        __VA_ARGS__                                                            \
    }                                                                          \
    if constexpr (opcode == cbltn) {                                           \
        if (currentFrame.iptr == programBreak) TERMINATE_EXECUTION();          \
    }                                                                          \
    currentFrame.iptr += ExecCodeSize<opcode>;                                 \
    /* Instead of going back to a loop header, we immediately jump to the      \
     * next instruction, significantly reducing branching */                   \
    GOTO_NEXT()

#include "ExecutionInstDef.h"

opcode_block_invalid:
    throwError<InvalidOpcodeError>((u64)*currentFrame.iptr);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#else  // JUMP_THREADING
    return executeNoJumpThread(start, arguments);
#endif // JUMP_THREADING
}

u64 const* VMImpl::executeNoJumpThread(size_t start,
                                       std::span<u64 const> arguments) {
    beginExecution(start, arguments);
    while (running()) {
        stepExecution();
    }
    return endExecution();
}

void VMImpl::beginExecution(size_t start, std::span<u64 const> arguments) {
    auto const lastframe = execFrames.top() = currentFrame;
    /// We add `MaxCallframeRegisterCount` to the register pointer because
    /// we have no way of knowing how many registers the currently running
    /// execution frame uses, so we have to assume the worst.
    currentFrame = execFrames.push(ExecutionFrame{
        .regPtr = lastframe.regPtr + VirtualMachine::MaxCallframeRegisterCount,
        .bottomReg =
            lastframe.regPtr + VirtualMachine::MaxCallframeRegisterCount,
        .iptr = binary + start,
        .stackPtr = lastframe.stackPtr });
    std::memcpy(currentFrame.regPtr, arguments.data(),
                arguments.size() * sizeof(u64));
}

bool VMImpl::running() const { return currentFrame.iptr < programBreak; }

void VMImpl::stepExecution() {
    OpCode const opcode = load<OpCode>(currentFrame.iptr);
    auto* const iptr = currentFrame.iptr + sizeof(OpCode);
    auto* const regPtr = currentFrame.regPtr;
    size_t codeOffset;
    switch ((u8)opcode) {
        using enum OpCode;
#define INST(opcode, ...)                                                      \
    case (u8)opcode:                                                           \
        codeOffset = ExecCodeSize<opcode>;                                     \
        __VA_ARGS__                                                            \
        break;

#define TERMINATE_EXECUTION() currentFrame.iptr = programBreak

#include "ExecutionInstDef.h"

        break;
    default:
        throwError<InvalidOpcodeError>((u64)opcode);
    }
    currentFrame.iptr += codeOffset;
    ++stats.executedInstructions;
}

u64 const* VMImpl::endExecution() {
    execFrames.pop();
    auto* result = currentFrame.regPtr;
    currentFrame = execFrames.top();
    return result;
}

size_t VMImpl::instructionPointerOffset() const {
    return utl::narrow_cast<size_t>(currentFrame.iptr - binary);
}

void VMImpl::setInstructionPointerOffset(size_t offset) {
    currentFrame.iptr = binary + offset;
}
