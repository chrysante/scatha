#include "Assembly2/Print.h"

#include <iomanip>
#include <iostream>

#include <utl/streammanip.hpp>
#include <utl/strcat.hpp>

#include "Assembly2/AssemblyStream.h"
#include "Assembly2/Elements.h"

using namespace scatha;
using namespace asm2;

void asm2::print(AssemblyStream const& assemblyStream) {
    print(assemblyStream, std::cout);
}

void asm2::print(AssemblyStream const& stream, std::ostream& str) {
    for (auto& elem: stream) {
        str << *elem << "\n";
    }
}

std::ostream& asm2::operator<<(std::ostream& str, Element const& elem) {
    return visit(elem, [&](auto& elem) -> auto& { return str << elem; });
}

static constexpr utl::streammanip instName = [](std::ostream& str, auto const&... args) {
    int const instNameWidth = 15;
    str << std::setw(instNameWidth) << std::left << utl::strcat(args...);
};


std::ostream& asm2::operator<<(std::ostream& str, MoveInst const& mov) {
    return str << instName("mov") << " " << mov.dest() << ", " << mov.source();
}

std::ostream& asm2::operator<<(std::ostream& str, ArithmeticInst const& arithmetic) {
    return str << instName(arithmetic.operation()) << " " << arithmetic.lhs() << ", " << arithmetic.rhs();
}

std::ostream& asm2::operator<<(std::ostream& str, JumpInst const& jmp) {
    return str << instName(toJumpInstName(jmp.condition())) << " " << jmp.targetLabelID();
}

std::ostream& asm2::operator<<(std::ostream& str, CallInst const& call) {
    return str << instName("call") << " " << call.functionLabelID();
}

std::ostream& asm2::operator<<(std::ostream& str, ReturnInst const&) {
    return str << instName("ret");
}

std::ostream& asm2::operator<<(std::ostream& str, CompareInst const& cmp) {
    return str << instName("cmp") << " " << cmp.lhs() << ", " << cmp.rhs();
}

std::ostream& asm2::operator<<(std::ostream& str, SetInst const& set) {
    return str << instName(toSetInstName(set.operation())) << set.dest();
}

std::ostream& asm2::operator<<(std::ostream& str, StoreRegAddress const& sra) {
    return str << instName("storeRegAddress") << " " << sra.dest() << ", &" << sra.source();
}

std::ostream& asm2::operator<<(std::ostream& str, Label const& label) {
    return str << label.name() << "/" << label.uniqueID();
}

std::ostream& asm2::operator<<(std::ostream& str, RegisterIndex const& regIdx) {
    return str << "_R[" << regIdx.value() << "]";
}

std::ostream& asm2::operator<<(std::ostream& str, MemoryAddress const& addr) {
    return str << "(ptr)_R[" << addr.registerIndex() << "]";
}

std::ostream& asm2::operator<<(std::ostream& str, Value const& value) {
    return str << value.value();
}
