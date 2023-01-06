#include "Assembly/Print.h"

#include <iomanip>
#include <iostream>

#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Assembly/AssemblyStream.h"

using namespace scatha;
using namespace Asm;

void Asm::print(AssemblyStream const& assemblyStream) {
    print(assemblyStream, std::cout);
}

void Asm::print(AssemblyStream const& stream, std::ostream& str) {
    for (auto& inst: stream) {
        str << inst << "\n";
    }
}

std::ostream& Asm::operator<<(std::ostream& str, Instruction const& inst) {
    return inst.visit([&](auto& inst) -> auto& { return str << inst; });
}

static constexpr utl::streammanip instName = [](std::ostream& str, auto const&... args) {
    int const instNameWidth = 8;
    str << "  " << std::setw(instNameWidth) << std::left << utl::strcat(args...);
};

std::ostream& Asm::operator<<(std::ostream& str, MoveInst const& mov) {
    return str << instName("mov", mov.numBytes()) << " " << mov.dest() << ", " << mov.source();
}

std::ostream& Asm::operator<<(std::ostream& str, UnaryArithmeticInst const& inst) {
    return str << instName(inst.operation()) << " " << inst.operand();
}

std::ostream& Asm::operator<<(std::ostream& str, ArithmeticInst const& inst) {
    return str << instName(inst.operation()) << " " << inst.dest() << ", " << inst.source();
}

std::ostream& Asm::operator<<(std::ostream& str, JumpInst const& jmp) {
    return str << instName(toJumpInstName(jmp.condition())) << " " << jmp.targetLabelID();
}

std::ostream& Asm::operator<<(std::ostream& str, CallInst const& call) {
    return str << instName("call") << " " << call.functionLabelID() << ", " << call.regPtrOffset();
}

std::ostream& Asm::operator<<(std::ostream& str, ReturnInst const&) {
    return str << instName("ret");
}

std::ostream& Asm::operator<<(std::ostream& str, TerminateInst const&) {
    return str << instName("terminate");
}

std::ostream& Asm::operator<<(std::ostream& str, CompareInst const& cmp) {
    return str << instName("cmp") << " " << cmp.lhs() << ", " << cmp.rhs();
}

std::ostream& Asm::operator<<(std::ostream& str, TestInst const& test) {
    return str << instName(test.type() == Type::Signed ? "itest" : "utest") << " " << test.operand();
}

std::ostream& Asm::operator<<(std::ostream& str, SetInst const& set) {
    return str << instName(toSetInstName(set.operation())) << set.dest();
}

std::ostream& Asm::operator<<(std::ostream& str, AllocaInst const& alloca_) {
    return str << instName("alloca") << " " << alloca_.dest() << ", &" << alloca_.source();
}

std::ostream& Asm::operator<<(std::ostream& str, Label const& label) {
    return str << label.name() << "/" << label.id() << ":";
}

std::ostream& Asm::operator<<(std::ostream& str, Value const& value) {
    return value.visit([&](auto& value) -> auto& { return str << value; });
}

std::ostream& Asm::operator<<(std::ostream& str, RegisterIndex const& regIdx) {
    return str << "_R[" << regIdx.value() << "]";
}

std::ostream& Asm::operator<<(std::ostream& str, MemoryAddress const& addr) {
    return str << "*(ptr)_R[" << addr.registerIndex() << "]";
}

std::ostream& Asm::operator<<(std::ostream& str, Value8 const& value) {
    return str << "(u8)" << value.value();
}

std::ostream& Asm::operator<<(std::ostream& str, Value16 const& value) {
    return str << "(u16)" << value.value();
}

std::ostream& Asm::operator<<(std::ostream& str, Value32 const& value) {
    return str << "(u32)" << value.value();
}

std::ostream& Asm::operator<<(std::ostream& str, Value64 const& value) {
    return str << "(u64)" << value.value();
}
