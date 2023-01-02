#define SC_TEST

#include <Catch/Catch2.hpp>

#include "Assembly2/Assembler.h"
#include "Assembly2/AssemblyStream.h"
#include "Basic/Memory.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace scatha::asm2;

static vm::VirtualMachine assembleAndExecute(AssemblyStream const& str) {
    vm::Program p = assemble(str);
    vm::VirtualMachine vm;
    vm.load(p);
    vm.execute();
    return vm;
}

[[maybe_unused]] static void assembleAndPrint(AssemblyStream const& str) {
    vm::Program p = assemble(str);
    print(p);
}

TEST_CASE("Alloca implementation", "[assembly][vm]") {
    AssemblyStream a;
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(0),
              std::make_unique<Value64>(128)));      // a = 128
    a.add(std::make_unique<StoreRegAddress>(
              std::make_unique<RegisterIndex>(1),
              std::make_unique<RegisterIndex>(2)));  // ptr = alloca(...)
    a.add(std::make_unique<MoveInst>(
              std::make_unique<MemoryAddress>(1, 0, 0),
              std::make_unique<RegisterIndex>(0)));  // *ptr = a
    a.add(std::make_unique<TerminateInst>());
    
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    
    CHECK(read<i64>(&state.registers[0]) == 128);
    CHECK(read<i64>(&state.registers[2]) == 128);
}

TEST_CASE("Euclidean algorithm", "[assembly][vm]") {
    enum { GCD };
    AssemblyStream a;
    // Should hold the result in R[2]
    // Main function
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(2),
              std::make_unique<Value64>(54)));     // a = 54
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(3),
              std::make_unique<Value64>(24)));     // b = 24
    a.add(std::make_unique<CallInst>(std::make_unique<Label>(GCD, "GCD"), 2));
    a.add(std::make_unique<TerminateInst>());
    // GCD function
    a.add(std::make_unique<Label>(GCD, "GCD"));
    a.add(std::make_unique<CompareInst>(
              Type::Signed,
              std::make_unique<RegisterIndex>(1),
              std::make_unique<Value64>(0)));      // Test b == 0
    a.add(std::make_unique<JumpInst>(CompareOperation::NotEq, std::make_unique<Label>(GCD + 1, "GCD - else")));
    a.add(std::make_unique<ReturnInst>());         // return a; (as it already is in R[0])
    a.add(std::make_unique<Label>(GCD + 1, "GCD - else"));
    // Swap a and b
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(2),
              std::make_unique<RegisterIndex>(1))); // c = b
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(1),
              std::make_unique<RegisterIndex>(0))); // b = a
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(0),
              std::make_unique<RegisterIndex>(2))); // a = c
    a.add(std::make_unique<ArithmeticInst>(
              ArithmeticOperation::Rem, Type::Signed,
              std::make_unique<RegisterIndex>(1),
              std::make_unique<RegisterIndex>(2)));
    a.add(std::make_unique<JumpInst>(std::make_unique<Label>(GCD, "GCD"))); // Tail call
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    // gcd(54, 24) == 6
    CHECK(state.registers[2] == 6);
}
//
//TEST_CASE("Euclidean algorithm no tail call", "[assembly][vm]") {
//    using enum Instruction;
//
//    enum { MAIN, GCD };
//
//    AssemblyStream a;
//
//    // Should hold the result in R[2]
//    a << Label(MAIN);                                  // Main function
//    a << enterFn << Value8(4);                        // allocate 4 registers
//                                                       // R[0] and R[1] are for the instruction pointer
//                                                       // and register pointer offset
//    a << mov << RegisterIndex(2) << Signed64(1023534); // R[2] = arg0
//    a << mov << RegisterIndex(3) << Signed64(213588);  // R[2] = arg1
//    a << call << Label(GCD) << Value8(2);
//
//    a << terminate;
//
//    a << Label(GCD);                              // gcd(i64 a, i64 b):
//    a << icmp << RegisterIndex(1) << Signed64(0); // b == 0
//    a << jne << Label(GCD, 0);
//    a << ret;
//
//    a << Label(GCD, 0);
//    a << enterFn << Value8(6);                        // Allocate 6 registers:
//                                                       // R[0]: a
//                                                       // R[1]: b
//                                                       // R[2]: rpOffset
//                                                       // R[3]: iptr
//                                                       // R[4]: b
//                                                       // R[5]: a % b
//                                                       // R[0] = a and R[1] = b have been placed by the caller.
//    a << mov << RegisterIndex(5) << RegisterIndex(0);  // R[5] = a
//    a << irem << RegisterIndex(5) << RegisterIndex(1); // R[5] %= b
//    a << mov << RegisterIndex(4) << RegisterIndex(1);  // R[4] = b
//    a << call << Label(GCD) << Value8(4);              // Deliberately no tail call
//    a << mov << RegisterIndex(0) << RegisterIndex(4);  // R[0] = R[4] to move the result to the expected register
//    a << ret;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    // gcd(1023534,213588) == 18
//    CHECK(state.registers[2] == 18);
//}
//

static void testArithmeticRR(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    AssemblyStream a;
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(0),
              std::make_unique<Value64>(arg1)));
    a.add(std::make_unique<MoveInst>(
              std::make_unique<RegisterIndex>(1),
              std::make_unique<Value64>(arg2)));
    a.add(std::make_unique<ArithmeticInst>(
              operation, type,
              std::make_unique<RegisterIndex>(0),
              std::make_unique<RegisterIndex>(1)));
    a.add(std::make_unique<TerminateInst>());
    auto const vm     = assembleAndExecute(a);
    auto const& state = vm.getState();
    CHECK(read<decltype(reference)>(&state.registers[0]) == reference);
}
//
//static void testArithmeticRV(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
//    using enum Instruction;
//
//    AssemblyStream a;
//    a << enterFn << Value8(1);
//    a << mov << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg1), type);
//    a << i << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg2), type);
//    a << terminate;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    CHECK(read<decltype(reference)>(state.regPtr) == reference);
//}
//
//static void testArithmeticRM(Instruction i, auto arg1, auto arg2, auto reference, Value64::Type type) {
//    using enum Instruction;
//
//    AssemblyStream a;
//    a << mov << RegisterIndex(0) << Value64(utl::bit_cast<u64>(arg1), type);
//    a << storeRegAddress << RegisterIndex(1) << RegisterIndex(3);
//    a << mov << RegisterIndex(2) << Value64(utl::bit_cast<u64>(arg2), type); // R[2] = arg2
//    a << mov << MemoryAddress(1, 0, 0) << RegisterIndex(2);
//    a << i << RegisterIndex(0) << MemoryAddress(1, 0, 0);
//    a << terminate;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    CHECK(read<decltype(reference)>(state.regPtr) == reference);
//}
//
static void testArithmetic(ArithmeticOperation operation, Type type, auto arg1, auto arg2, auto reference) {
    testArithmeticRR(operation, type, arg1, arg2, reference);
//    testArithmeticRV(operation, type, arg1, arg2, reference);
//    testArithmeticRM(operation, type, arg1, arg2, reference);
}
//
TEST_CASE("Arithmetic", "[assembly][vm]") {
    SECTION("add") {
        testArithmetic(ArithmeticOperation::Add, Type::Unsigned, 6,    2,   8);
        testArithmetic(ArithmeticOperation::Add, Type::Signed,   2,   -6,  -4);
        testArithmetic(ArithmeticOperation::Add, Type::Float,    6.4, -2.2, 4.2);
    }
    SECTION("sub") {
        testArithmetic(ArithmeticOperation::Sub, Type::Unsigned, 6,    2,   4);
        testArithmetic(ArithmeticOperation::Sub, Type::Signed,   2,   -6,   8);
        testArithmetic(ArithmeticOperation::Sub, Type::Float,    6.0,  2.3, 3.7);
    }
    SECTION("mul") {
        testArithmetic(ArithmeticOperation::Mul, Type::Unsigned, 6,   2,   12);
        testArithmetic(ArithmeticOperation::Mul, Type::Signed,   2,  -6,  -12);
        testArithmetic(ArithmeticOperation::Mul, Type::Float,    2.4, 2.5,  6.0);
    }
    SECTION("div") {
        testArithmetic(ArithmeticOperation::Div, Type::Unsigned,   6,   2,   3);
        testArithmetic(ArithmeticOperation::Div, Type::Unsigned, 100,   3,  33);
        testArithmetic(ArithmeticOperation::Div, Type::Signed,     6,  -2,  -3);
        testArithmetic(ArithmeticOperation::Div, Type::Signed,   100,  -3, -33);
        testArithmetic(ArithmeticOperation::Div, Type::Float,      6.3, 3.0, 2.1);
    }
    SECTION("rem") {
        testArithmetic(ArithmeticOperation::Rem, Type::Unsigned,   6,  2,  0);
        testArithmetic(ArithmeticOperation::Rem, Type::Unsigned, 100,  3,  1);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed,     6, -2,  0);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed,   100, -3,  1);
        testArithmetic(ArithmeticOperation::Rem, Type::Signed,  -100,  3, -1);
    }
}
//
//TEST_CASE("Unconditional jump", "[assembly][vm]") {
//    using enum Instruction;
//
//    u64 const value = GENERATE(0u, 1u, 2u, 3u);
//
//    AssemblyStream a;
//
//    a << enterFn << Value8(1); // allocate 1 register
//    a << jmp << Label(0, value);
//    a << Label(0, 0);
//    a << mov << RegisterIndex(0) << Signed64(0);
//    a << terminate;
//    a << Label(0, 1);
//    a << mov << RegisterIndex(0) << Signed64(1);
//    a << terminate;
//    a << Label(0, 2);
//    a << mov << RegisterIndex(0) << Signed64(2);
//    a << terminate;
//    a << Label(0, 3);
//    a << mov << RegisterIndex(0) << Signed64(3);
//    a << terminate;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    CHECK(read<u64>(state.regPtr) == value);
//}
//
//TEST_CASE("Conditional jump", "[assembly][vm]") {
//    u64 const value = GENERATE(0u, 1u, 2u, 3u);
//    i64 const arg1  = GENERATE(-2, 0, 5, 100);
//    i64 const arg2  = GENERATE(-100, -3, 0, 7);
//
//    AssemblyStream a;
//
//    using enum Instruction;
//    a << enterFn << Value8(2);
//    a << mov << RegisterIndex(0) << Signed64(arg1);
//    a << icmp << RegisterIndex(0) << Signed64(arg2);
//    a << jle << Label(0, value);
//    a << mov << RegisterIndex(1) << Signed64(-1);
//    a << terminate;
//    a << Label(0, 0);
//    a << mov << RegisterIndex(1) << Signed64(0);
//    a << terminate;
//    a << Label(0, 1);
//    a << mov << RegisterIndex(1) << Signed64(1);
//    a << terminate;
//    a << Label(0, 2);
//    a << mov << RegisterIndex(1) << Signed64(2);
//    a << terminate;
//    a << Label(0, 3);
//    a << mov << RegisterIndex(1) << Signed64(3);
//    a << terminate;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    CHECK(read<u64>(state.regPtr + 1) == (arg1 <= arg2 ? value : static_cast<u64>(-1)));
//}
//
//TEST_CASE("itest, set*", "[assembly][vm]") {
//    AssemblyStream a;
//
//    using enum Instruction;
//    a << enterFn << Value8(6);
//    a << mov << RegisterIndex(0) << Signed64(-1);
//    a << itest << RegisterIndex(0);
//    a << sete << RegisterIndex(0);
//    a << setne << RegisterIndex(1);
//    a << setl << RegisterIndex(2);
//    a << setle << RegisterIndex(3);
//    a << setg << RegisterIndex(4);
//    a << setge << RegisterIndex(5);
//    a << terminate;
//
//    auto const vm     = assembleAndExecute(a);
//    auto const& state = vm.getState();
//
//    CHECK(state.registers[0] == 0);
//    CHECK(state.registers[1] == 1);
//    CHECK(state.registers[2] == 1);
//    CHECK(state.registers[3] == 1);
//    CHECK(state.registers[4] == 0);
//    CHECK(state.registers[5] == 0);
//}
