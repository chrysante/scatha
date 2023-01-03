#include "Assembly2/Print.h"

#include <iomanip>
#include <iostream>

#include <utl/streammanip.hpp>
#include <utl/strcat.hpp>

#include "Assembly2/AssemblyStream.h"

using namespace scatha;
using namespace asm2;

void asm2::print(AssemblyStream const& assemblyStream) {
    print(assemblyStream, std::cout);
}

void asm2::print(AssemblyStream const& stream, std::ostream& str) {
    for (auto& inst: stream) {
        str << inst << "\n";
    }
}

std::ostream& asm2::operator<<(std::ostream& str, Instruction const& inst) {
    return inst.visit([&](auto& inst) -> auto& { return str << inst; });
}

static constexpr utl::streammanip instName = [](std::ostream& str, auto const&... args) {
    int const instNameWidth = 8;
    str << "  " << std::setw(instNameWidth) << std::left << utl::strcat(args...);
};

std::ostream& asm2::operator<<(std::ostream& str, MoveInst const& mov) {
    return str << instName("mov") << " " << mov.dest() << ", " << mov.source();
}

std::ostream& asm2::operator<<(std::ostream& str, UnaryArithmeticInst const& inst) {
    return str << instName(inst.operation()) << " " << inst.operand();
}

std::ostream& asm2::operator<<(std::ostream& str, ArithmeticInst const& inst) {
    return str << instName(inst.operation()) << " " << inst.dest() << ", " << inst.source();
}

std::ostream& asm2::operator<<(std::ostream& str, JumpInst const& jmp) {
    return str << instName(toJumpInstName(jmp.condition())) << " " << jmp.targetLabelID();
}

std::ostream& asm2::operator<<(std::ostream& str, CallInst const& call) {
    return str << instName("call") << " " << call.functionLabelID() << ", " << call.regPtrOffset();
}

std::ostream& asm2::operator<<(std::ostream& str, ReturnInst const&) {
    return str << instName("ret");
}

std::ostream& asm2::operator<<(std::ostream& str, CompareInst const& cmp) {
    return str << instName("cmp") << " " << cmp.lhs() << ", " << cmp.rhs();
}

std::ostream& asm2::operator<<(std::ostream& str, TestInst const& test) {
    return str << instName(test.type() == Type::Signed ? "itest" : "utest") << " " << test.operand();
}

std::ostream& asm2::operator<<(std::ostream& str, SetInst const& set) {
    return str << instName(toSetInstName(set.operation())) << set.dest();
}

std::ostream& asm2::operator<<(std::ostream& str, AllocaInst const& alloca_) {
    return str << instName("alloca") << " " << alloca_.dest() << ", &" << alloca_.source();
}

std::ostream& asm2::operator<<(std::ostream& str, Label const& label) {
    return str << label.name() << "/" << label.id() << ":";
}

std::ostream& asm2::operator<<(std::ostream& str, Value const& value) {
    return value.visit([&](auto& value) -> auto& { return str << value; });
}

std::ostream& asm2::operator<<(std::ostream& str, RegisterIndex const& regIdx) {
    return str << "_R[" << regIdx.value() << "]";
}

std::ostream& asm2::operator<<(std::ostream& str, MemoryAddress const& addr) {
    return str << "*(ptr)_R[" << addr.registerIndex() << "]";
}

std::ostream& asm2::operator<<(std::ostream& str, Value8 const& value) {
    return str << value.value();
}

std::ostream& asm2::operator<<(std::ostream& str, Value16 const& value) {
    return str << value.value();
}

std::ostream& asm2::operator<<(std::ostream& str, Value32 const& value) {
    return str << value.value();
}

std::ostream& asm2::operator<<(std::ostream& str, Value64 const& value) {
    return str << value.value();
}
