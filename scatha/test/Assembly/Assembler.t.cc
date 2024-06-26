#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <svm/Builtin.h>
#include <svm/Program.h>
#include <svm/VirtualMachine.h>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"
#include "Util/CoutRerouter.h"

using namespace scatha;
using namespace Asm;

template <typename T>
static T load(void const* ptr) {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage;
    std::memcpy(&storage, ptr, sizeof(T));
    return reinterpret_cast<T const&>(storage);
}

static auto assembleAndExecute(AssemblyStream const& str) {
    auto [prog, sym, unresolved] = assemble(str);
    if (!link(LinkerOptions{}, prog, {}, unresolved)) {
        throw std::runtime_error("Linker error");
    }
    svm::VirtualMachine vm(1024, 1024);
    vm.loadBinary(prog.data());
    vm.execute(0, {});
    return std::pair{ vm.registerData() | ranges::to<std::vector>,
                      vm.stackData() | ranges::to<std::vector> };
}

[[maybe_unused]] static void assembleAndPrint(AssemblyStream const& str) {
    auto [prog, sym, unresolved] = assemble(str);
    if (!link(LinkerOptions{}, prog, {}, unresolved)) {
        throw std::runtime_error("Linker error");
    }
    svm::print(prog.data());
}

TEST_CASE("Alloca implementation", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(128), 8),     // a = 128
        LIncSPInst(RegisterIndex(1), Value16(8)),        // ptr = alloca(8)
        MoveInst(MemoryAddress(1), RegisterIndex(0), 8), // *ptr = a
        TerminateInst()
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(regs[0] == 128);
    CHECK(stack[0] == 128);
}

TEST_CASE("Alloca 2", "[assembly][vm]") {
    size_t const offset = GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(1), 8),      // a = 128
        LIncSPInst(RegisterIndex(1), Value16(8)),       // ptr = alloca(8)
        MoveInst(MemoryAddress(1, MemoryAddress::InvalidRegisterIndex.value(), 0, offset),
                 RegisterIndex(0), 1),                  // ptr[offset] = a
        TerminateInst()
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CAPTURE(offset);
    CHECK(stack[offset] == 1);
}

TEST_CASE("Euclidean algorithm", "[assembly][vm]") {
    LabelID const main{ 0 };
    LabelID const gcd{ 1 };
    LabelID const gcd_else{ 2 };
    AssemblyStream a;
    // Main function
    // Should hold the result in R[3]
    // clang-format off
    a.add(Block(main, "main", {
        MoveInst(RegisterIndex(3), Value64(54), 8), // a = 54
        MoveInst(RegisterIndex(4), Value64(24), 8), // b = 24
        CallInst(LabelPosition(gcd), 3),
        TerminateInst()
    }));
    // GCD function
    a.add(Block(gcd, "gcd", {
        CompareInst(Type::Signed, RegisterIndex(1), Value64(0), 8), // Test b == 0
        JumpInst(CompareOperation::NotEq, gcd_else),
        ReturnInst() // return a; (as it already is in R[0])
    }));
    a.add(Block(gcd_else, "gcd-else", {
        // Swap a and b
        MoveInst(RegisterIndex(2), RegisterIndex(1), 8), // c = b
        MoveInst(RegisterIndex(1), RegisterIndex(0), 8), // b = a
        MoveInst(RegisterIndex(0), RegisterIndex(2), 8), // a = c
        ArithmeticInst(ArithmeticOperation::SRem, RegisterIndex(1), RegisterIndex(2), 8),
        JumpInst(gcd) // Tail call
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    // gcd(54, 24) == 6
    CHECK(regs[3] == 6);
}

TEST_CASE("Euclidean algorithm no tail call", "[assembly][vm]") {
    LabelID const main{ 0 };
    LabelID const gcd{ 1 };
    LabelID const gcd_else{ 2 };
    AssemblyStream a;
    // Should hold the result in R[3]
    // clang-format off
    a.add(Block(main, "main", {
        MoveInst(RegisterIndex(3), Value64(1023534), 8), // R[2] = arg0
        MoveInst(RegisterIndex(4), Value64(213588), 8),  // R[2] = arg1
        CallInst(LabelPosition(gcd), 3),
        TerminateInst(),
    }));
    a.add(Block(gcd, "gcd", {
        CompareInst(Type::Signed, RegisterIndex(1), Value64(0), 8), // b == 0
        JumpInst(CompareOperation::NotEq, gcd_else),
        ReturnInst(),
    }));
    a.add(Block(gcd_else, "gcd-else", {
        // R[0]: a
        // R[1]: b
        // R[2]: rpOffset
        // R[3]: iptr
        // R[4]: b
        // R[5]: a % b
        // R[0] = a and R[1] = b have been placed by the caller.
        MoveInst(RegisterIndex(6), RegisterIndex(0), 8),                                  // R[6] = a
        ArithmeticInst(ArithmeticOperation::SRem, RegisterIndex(6), RegisterIndex(1), 8), // R[6] %= b
        MoveInst(RegisterIndex(5), RegisterIndex(1), 8),                                  // R[5] = b
        CallInst(LabelPosition(gcd), 5),                 // Deliberately no tail call
        MoveInst(RegisterIndex(0), RegisterIndex(5), 8), // R[0] = R[5] to move the result to the expected register
        ReturnInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    // gcd(1023534,213588) == 18
    CHECK(regs[3] == 18);
}

static void testArithmeticRR(ArithmeticOperation operation, auto arg1,
                             auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        MoveInst(RegisterIndex(1), Value64(arg2), 8),
        ArithmeticInst(operation, RegisterIndex(0), RegisterIndex(1), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<decltype(reference)>(&regs[0]) == reference);
}

static void testArithmeticRV(ArithmeticOperation operation, auto arg1,
                             auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        ArithmeticInst(operation, RegisterIndex(0), Value64(arg2), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<decltype(reference)>(&regs[0]) == reference);
}

static void testArithmeticRM(ArithmeticOperation operation, auto arg1,
                             auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        MoveInst(RegisterIndex(1), Value64(arg2), 8),
        LIncSPInst(RegisterIndex(2), Value16(8)),
        MoveInst(MemoryAddress(2), RegisterIndex(1), 8),
        ArithmeticInst(operation, RegisterIndex(0), MemoryAddress(2), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<decltype(reference)>(&regs[0]) == reference);
}

static void testArithmetic(ArithmeticOperation operation, auto arg1, auto arg2,
                           auto reference) {
    testArithmeticRR(operation, arg1, arg2, reference);
    testArithmeticRV(operation, arg1, arg2, reference);
    testArithmeticRM(operation, arg1, arg2, reference);
}

TEST_CASE("Arithmetic", "[assembly][vm]") {
    SECTION("add") {
        testArithmetic(ArithmeticOperation::Add, 6, 2, 8);
        testArithmetic(ArithmeticOperation::Add, 2, -6, -4);
        testArithmetic(ArithmeticOperation::FAdd, 6.4, -2.2, 4.2);
    }
    SECTION("sub") {
        testArithmetic(ArithmeticOperation::Sub, 6, 2, 4);
        testArithmetic(ArithmeticOperation::Sub, 2, -6, 8);
        testArithmetic(ArithmeticOperation::FSub, 6.0, 2.3, 3.7);
    }
    SECTION("mul") {
        testArithmetic(ArithmeticOperation::Mul, 6, 2, 12);
        testArithmetic(ArithmeticOperation::Mul, 2, -6, -12);
        testArithmetic(ArithmeticOperation::FMul, 2.4, 2.5, 6.0);
    }
    SECTION("div") {
        testArithmetic(ArithmeticOperation::UDiv, 6, 2, 3);
        testArithmetic(ArithmeticOperation::UDiv, 100, 3, 33);
        testArithmetic(ArithmeticOperation::SDiv, 6, -2, -3);
        testArithmetic(ArithmeticOperation::SDiv, 100, -3, -33);
        testArithmetic(ArithmeticOperation::FDiv, 6.3, 3.0, 2.1);
    }
    SECTION("rem") {
        testArithmetic(ArithmeticOperation::URem, 6, 2, 0);
        testArithmetic(ArithmeticOperation::URem, 100, 3, 1);
        testArithmetic(ArithmeticOperation::SRem, 6, -2, 0);
        testArithmetic(ArithmeticOperation::SRem, 100, -3, 1);
        testArithmetic(ArithmeticOperation::SRem, -100, 3, -1);
    }
}

TEST_CASE("Unconditional jump", "[assembly][vm]") {
    LabelID const dest{ GENERATE(1u, 2u, 3u, 4u) };
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        JumpInst(dest)
    }));
    a.add(Block(LabelID{ 1 }, "1", {
        MoveInst(RegisterIndex(0), Value64(1), 8),
        TerminateInst()
    }));
    a.add(Block(LabelID{ 2 }, "2", {
        MoveInst(RegisterIndex(0), Value64(2), 8),
        TerminateInst()
    }));
    a.add(Block(LabelID{ 3 }, "3", {
        MoveInst(RegisterIndex(0), Value64(3), 8),
        TerminateInst()
    }));
    a.add(Block(LabelID{ 4 }, "4", {
        MoveInst(RegisterIndex(0), Value64(4), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(regs[0] == utl::to_underlying(dest));
}

TEST_CASE("Conditional jump", "[assembly][vm]") {
    LabelID const dest{ GENERATE(1u, 2u, 3u, 4u) };
    i64 const arg1 = GENERATE(-2, 0, 5, 100);
    i64 const arg2 = GENERATE(-100, -3, 0, 7);
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        CompareInst(Type::Signed, RegisterIndex(0), Value64(arg2), 8),
        JumpInst(CompareOperation::LessEq, dest),
        MoveInst(RegisterIndex(1), Value64(-1), 8),
        TerminateInst(),
    }));
    a.add(Block(LabelID{ 1 }, "1", {
        MoveInst(RegisterIndex(1), Value64(1), 8),
        TerminateInst(),
    }));
    a.add(Block(LabelID{ 2 }, "2", {
        MoveInst(RegisterIndex(1), Value64(2), 8),
        TerminateInst()
    }));
    a.add(Block(LabelID{ 3 }, "3", {
        MoveInst(RegisterIndex(1), Value64(3), 8),
        TerminateInst()
    }));
    a.add(Block(LabelID{ 4 }, "4", {
        MoveInst(RegisterIndex(1), Value64(4), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<u64>(&regs[1]) ==
          (arg1 <= arg2 ? utl::to_underlying(dest) : static_cast<u64>(-1)));
}

TEST_CASE("itest, set*", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(-1), 8),
        TestInst(Type::Signed, RegisterIndex(0), 8),
        SetInst(RegisterIndex(0), CompareOperation::Eq),
        SetInst(RegisterIndex(1), CompareOperation::NotEq),
        SetInst(RegisterIndex(2), CompareOperation::Less),
        SetInst(RegisterIndex(3), CompareOperation::LessEq),
        SetInst(RegisterIndex(4), CompareOperation::Greater),
        SetInst(RegisterIndex(5), CompareOperation::GreaterEq),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(regs[0] == 0);
    CHECK(regs[1] == 1);
    CHECK(regs[2] == 1);
    CHECK(regs[3] == 1);
    CHECK(regs[4] == 0);
    CHECK(regs[5] == 0);
}

TEST_CASE("callExt", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(-1), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_puti64",
                                             std::array{ FFIType::Int64() },
                                             FFIType::Void())),
        MoveInst(RegisterIndex(0), Value64(' '), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_putchar",
                                             std::array{ FFIType::Int8() },
                                             FFIType::Void())),
        MoveInst(RegisterIndex(0), Value64('X'), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_putchar",
                                             std::array{ FFIType::Int8() },
                                             FFIType::Void())),
        MoveInst(RegisterIndex(0), Value64(' '), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_putchar",
                                             std::array{ FFIType::Int8() },
                                             FFIType::Void())),
        MoveInst(RegisterIndex(0), Value64(0.5), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_putf64",
                                             std::array{ FFIType::Double() },
                                             FFIType::Void())),
        TerminateInst()
    })); // clang-format on
    test::CoutRerouter cr;
    assembleAndExecute(a);
    CHECK(cr.str() == "-1 X 0.5");
}

TEST_CASE("callExt with return value", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(2.0), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    ForeignFunctionInterface("__builtin_sqrt_f64",
                                             std::array{ FFIType::Double() },
                                             FFIType::Double())),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(regs[0] == utl::bit_cast<u64>(std::sqrt(2.0)));
}

TEST_CASE("Conditional move", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(2), 8),
        MoveInst(RegisterIndex(1), Value64(0), 8),
        TestInst(Type::Unsigned, RegisterIndex(1), 8),
        CMoveInst(CompareOperation::Eq, RegisterIndex(0), Value64(42), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(regs[0] == 42);
}

TEST_CASE("LEA instruction", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        LIncSPInst(RegisterIndex(0), Value16(80)),
        MoveInst(RegisterIndex(1), Value64(2), 8),
        LEAInst(RegisterIndex(2), MemoryAddress(0, 1, 16, 8)),
        MoveInst(RegisterIndex(0), Value64(42), 8),
        MoveInst(MemoryAddress(2), RegisterIndex(0), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<u64>(&stack[40]) == 42);
}

TEST_CASE("cmov instruction", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(LabelID{ 0 }, "start", {
        MoveInst(RegisterIndex(0), Value64(5), 8),
        MoveInst(RegisterIndex(1), Value64(7), 8),
        CompareInst(Type::Signed, RegisterIndex(0), Value64(0), 8),
        CMoveInst(CompareOperation::Eq, RegisterIndex(0), RegisterIndex(1), 8),
        TerminateInst(),
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    CHECK(load<u64>(&regs[0]) == 5);
}

TEST_CASE("icall register instruction", "[assembly][vm]") {
    LabelID const main{ 0 };
    LabelID const func{ 1 };
    AssemblyStream a;
    // Main function
    // clang-format off
    a.add(Block(main, "main", {
        MoveInst(RegisterIndex(1),
                 LabelPosition(func, LabelPosition::Dynamic),
                 8),
        MoveInst(RegisterIndex(3), Value64(13), 8),
        MoveInst(RegisterIndex(4), Value64(29), 8),
        CallInst(RegisterIndex(1), 3),
        MoveInst(RegisterIndex(0), RegisterIndex(3), 8), 
        TerminateInst()
    }));
    // Callee
    a.add(Block(func, "func", {
        ArithmeticInst(ArithmeticOperation::Add,
                       RegisterIndex(0),
                       RegisterIndex(1), 8),
        ReturnInst()
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    // 13 + 29 == 42
    CHECK(regs[0] == 42);
}

TEST_CASE("icall memory instruction", "[assembly][vm]") {
    LabelID const main{ 0 };
    LabelID const func{ 1 };
    AssemblyStream a;
    // Main function
    // clang-format off
    a.add(Block(main, "main", {
        LIncSPInst(RegisterIndex(0), Value16(16)), // %0 = alloca(8)
        MoveInst(RegisterIndex(1),
                 LabelPosition(func, LabelPosition::Dynamic),
                 8),
        MoveInst(MemoryAddress(0, 0xFF, 0, 8), RegisterIndex(1), 8),
        MoveInst(RegisterIndex(3), Value64(13), 8),
        MoveInst(RegisterIndex(4), Value64(29), 8),
        CallInst(MemoryAddress(0, 0xFF, 0, 8), 3),
        MoveInst(RegisterIndex(0), RegisterIndex(3), 8),
        TerminateInst()
    }));
    // Callee
    a.add(Block(func, "func", {
        ArithmeticInst(ArithmeticOperation::Add,
                       RegisterIndex(0),
                       RegisterIndex(1), 8),
        ReturnInst()
    })); // clang-format on
    auto const [regs, stack] = assembleAndExecute(a);
    // 13 + 29 == 42
    CHECK(regs[0] == 42);
}
