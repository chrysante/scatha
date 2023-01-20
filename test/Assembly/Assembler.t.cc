#include <Catch/Catch2.hpp>

#include <svm/Builtin.h>
#include <svm/VirtualMachine.h>
#include <svm/Program.h>

#include "Assembly/Assembler.h"
#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"
#include "Basic/Memory.h"
#include "test/CoutRerouter.h"

using namespace scatha;
using namespace scatha::Asm;

static svm::VirtualMachine assembleAndExecute(AssemblyStream const& str) {
    auto p = assemble(str);
    svm::VirtualMachine vm;
    vm.loadProgram(p.data());
    vm.execute();
    return vm;
}

[[maybe_unused]] static void assembleAndPrint(AssemblyStream const& str) {
    auto p = assemble(str);
    svm::print(p.data());
}

TEST_CASE("Alloca implementation", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(128), 8),     // a = 128
        AllocaInst(RegisterIndex(1), RegisterIndex(2)),  // ptr = alloca(...)
        MoveInst(MemoryAddress(1), RegisterIndex(0), 8), // *ptr = a
        TerminateInst()
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<i64>(&state.registers[0]) == 128);
    CHECK(read<i64>(&state.registers[2]) == 128);
}

TEST_CASE("Alloca 2", "[assembly][vm]") {
    int const offset = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(1), 8),      // a = 128
        AllocaInst(RegisterIndex(1), RegisterIndex(2)), // ptr = alloca(...)
        MoveInst(MemoryAddress(1, MemoryAddress::invalidRegisterIndex, 0, offset),
                 RegisterIndex(0), 1), // ptr[offset] = a
        TerminateInst()
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<i64>(&state.registers[2]) == i64(1) << 8 * offset);
}

TEST_CASE("Euclidean algorithm", "[assembly][vm]") {
    enum { main, gcd, gcd_else };
    AssemblyStream a;
    // Main function
    // Should hold the result in R[2]
    // clang-format off
    a.add(Block(main, "main", {
        MoveInst(RegisterIndex(2), Value64(54), 8), // a = 54
        MoveInst(RegisterIndex(3), Value64(24), 8), // b = 24
        CallInst(gcd, 2),
        TerminateInst()
    }));
    // GCD function
    a.add(Block(gcd, "gcd", {
        CompareInst(Type::Signed, RegisterIndex(1), Value64(0)), // Test b == 0
        JumpInst(CompareOperation::NotEq, gcd_else),
        ReturnInst() // return a; (as it already is in R[0])
    }));
    a.add(Block(gcd_else, "gcd-else", {
        // Swap a and b
        MoveInst(RegisterIndex(2), RegisterIndex(1), 8), // c = b
        MoveInst(RegisterIndex(1), RegisterIndex(0), 8), // b = a
        MoveInst(RegisterIndex(0), RegisterIndex(2), 8), // a = c
        ArithmeticInst(ArithmeticOperation::Rem, Type::Signed, RegisterIndex(1), RegisterIndex(2)),
        JumpInst(gcd) // Tail call
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    // gcd(54, 24) == 6
    CHECK(state.registers[2] == 6);
}

TEST_CASE("Euclidean algorithm no tail call", "[assembly][vm]") {
    enum { main, gcd, gcd_else };
    AssemblyStream a;
    // Should hold the result in R[2]
    // clang-format off
    a.add(Block(main, "main", {
        MoveInst(RegisterIndex(2), Value64(1023534), 8), // R[2] = arg0
        MoveInst(RegisterIndex(3), Value64(213588), 8),  // R[2] = arg1
        CallInst(gcd, 2),
        TerminateInst(),
    }));
    a.add(Block(gcd, "gcd", {
        CompareInst(Type::Signed, RegisterIndex(1), Value64(0)), // b == 0
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
        MoveInst(RegisterIndex(5), RegisterIndex(0), 8),                                            // R[5] = a
        ArithmeticInst(ArithmeticOperation::Rem, Type::Signed, RegisterIndex(5), RegisterIndex(1)), // R[5] %= b
        MoveInst(RegisterIndex(4), RegisterIndex(1), 8),                                            // R[4] = b
        CallInst(gcd, 4),                                // Deliberately no tail call
        MoveInst(RegisterIndex(0), RegisterIndex(4), 8), // R[0] = R[4] to move the result to the expected register
        ReturnInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    // gcd(1023534,213588) == 18
    CHECK(state.registers[2] == 18);
}

static void testArithmeticRR(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        MoveInst(RegisterIndex(1), Value64(arg2), 8),
        ArithmeticInst(operation, type, RegisterIndex(0), RegisterIndex(1)),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<decltype(reference)>(&state.registers[0]) == reference);
}

static void testArithmeticRV(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        ArithmeticInst(operation, type, RegisterIndex(0), Value64(arg2)),
        TerminateInst(),
    })); // clang-format on

    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<decltype(reference)>(&state.registers[0]) == reference);
}

static void testArithmeticRM(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        MoveInst(RegisterIndex(1), Value64(arg2), 8),
        AllocaInst(RegisterIndex(2), RegisterIndex(3)),
        MoveInst(MemoryAddress(2), RegisterIndex(1), 8),
        ArithmeticInst(operation, type, RegisterIndex(0), MemoryAddress(2)),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<decltype(reference)>(&state.registers[0]) == reference);
}

static void testArithmetic(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    testArithmeticRR(operation, type, arg1, arg2, reference);
    testArithmeticRV(operation, type, arg1, arg2, reference);
    testArithmeticRM(operation, type, arg1, arg2, reference);
}

TEST_CASE("Arithmetic", "[assembly][vm]") {
    SECTION("add") {
        testArithmetic(ArithmeticOperation::Add, Type::Unsigned, 6, 2, 8);
        testArithmetic(ArithmeticOperation::Add, Type::Signed, 2, -6, -4);
        testArithmetic(ArithmeticOperation::Add, Type::Float, 6.4, -2.2, 4.2);
    }
    SECTION("sub") {
        testArithmetic(ArithmeticOperation::Sub, Type::Unsigned, 6, 2, 4);
        testArithmetic(ArithmeticOperation::Sub, Type::Signed, 2, -6, 8);
        testArithmetic(ArithmeticOperation::Sub, Type::Float, 6.0, 2.3, 3.7);
    }
    SECTION("mul") {
        testArithmetic(ArithmeticOperation::Mul, Type::Unsigned, 6, 2, 12);
        testArithmetic(ArithmeticOperation::Mul, Type::Signed, 2, -6, -12);
        testArithmetic(ArithmeticOperation::Mul, Type::Float, 2.4, 2.5, 6.0);
    }
    SECTION("div") {
        testArithmetic(ArithmeticOperation::Div, Type::Unsigned, 6, 2, 3);
        testArithmetic(ArithmeticOperation::Div, Type::Unsigned, 100, 3, 33);
        testArithmetic(ArithmeticOperation::Div, Type::Signed, 6, -2, -3);
        testArithmetic(ArithmeticOperation::Div, Type::Signed, 100, -3, -33);
        testArithmetic(ArithmeticOperation::Div, Type::Float, 6.3, 3.0, 2.1);
    }
    SECTION("rem") {
        testArithmetic(ArithmeticOperation::Rem, Type::Unsigned, 6, 2, 0);
        testArithmetic(ArithmeticOperation::Rem, Type::Unsigned, 100, 3, 1);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed, 6, -2, 0);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed, 100, -3, 1);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed, -100, 3, -1);
    }
}

TEST_CASE("Unconditional jump", "[assembly][vm]") {
    u64 const value = GENERATE(1u, 2u, 3u, 4u);
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        JumpInst(value)
    }));
    a.add(Block(1, "1", {
        MoveInst(RegisterIndex(0), Value64(1), 8),
        TerminateInst()
    }));
    a.add(Block(2, "2", {
        MoveInst(RegisterIndex(0), Value64(2), 8),
        TerminateInst()
    }));
    a.add(Block(3, "3", {
        MoveInst(RegisterIndex(0), Value64(3), 8),
        TerminateInst()
    }));
    a.add(Block(4, "4", {
        MoveInst(RegisterIndex(0), Value64(4), 8),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<u64>(state.regPtr) == value);
}

TEST_CASE("Conditional jump", "[assembly][vm]") {
    u64 const value = GENERATE(1u, 2u, 3u, 4u);
    i64 const arg1  = GENERATE(-2, 0, 5, 100);
    i64 const arg2  = GENERATE(-100, -3, 0, 7);
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(arg1), 8),
        CompareInst(Type::Signed, RegisterIndex(0), Value64(arg2)),
        JumpInst(CompareOperation::LessEq, value),
        MoveInst(RegisterIndex(1), Value64(-1), 8),
        TerminateInst(),
    }));
    a.add(Block(1, "1", {
        MoveInst(RegisterIndex(1), Value64(1), 8),
        TerminateInst(),
    }));
    a.add(Block(2, "2", {
        MoveInst(RegisterIndex(1), Value64(2), 8),
        TerminateInst()
    }));
    a.add(Block(3, "3", {
        MoveInst(RegisterIndex(1), Value64(3), 8),
        TerminateInst()
    }));
    a.add(Block(4, "4", {
        MoveInst(RegisterIndex(1), Value64(4), 8),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<u64>(&state.registers[1]) == (arg1 <= arg2 ? value : static_cast<u64>(-1)));
}

TEST_CASE("itest, set*", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(-1), 8),
        TestInst(Type::Signed, RegisterIndex(0)),
        SetInst(RegisterIndex(0), CompareOperation::Eq),
        SetInst(RegisterIndex(1), CompareOperation::NotEq),
        SetInst(RegisterIndex(2), CompareOperation::Less),
        SetInst(RegisterIndex(3), CompareOperation::LessEq),
        SetInst(RegisterIndex(4), CompareOperation::Greater),
        SetInst(RegisterIndex(5), CompareOperation::GreaterEq),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(state.registers[0] == 0);
    CHECK(state.registers[1] == 1);
    CHECK(state.registers[2] == 1);
    CHECK(state.registers[3] == 1);
    CHECK(state.registers[4] == 0);
    CHECK(state.registers[5] == 0);
}

TEST_CASE("callExt", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(-1), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::puti64)),
        MoveInst(RegisterIndex(0), Value64(' '), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::putchar)),
        MoveInst(RegisterIndex(0), Value64('X'), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::putchar)),
        MoveInst(RegisterIndex(0), Value64(' '), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::putchar)),
        MoveInst(RegisterIndex(0), Value64(0.5), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::putf64)),
        TerminateInst()
    })); // clang-format on
    CoutRerouter cr;
    assembleAndExecute(a);
    CHECK(cr.str() == "-1 X 0.5");
}

TEST_CASE("callExt with return value", "[assembly][vm]") {
    AssemblyStream a;
    // clang-format off
    a.add(Block(0, "start", {
        MoveInst(RegisterIndex(0), Value64(2.0), 8),
        CallExtInst(/* regPtrOffset = */ 0,
                    svm::builtinFunctionSlot,
                    /* index = */ static_cast<size_t>(svm::Builtin::sqrtf64)),
        TerminateInst(),
    })); // clang-format on
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(state.registers[0] == utl::bit_cast<u64>(std::sqrt(2.0)));
}
