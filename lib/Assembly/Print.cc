#include "Assembly/Print.h"

#include <iomanip>
#include <iostream>

#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"

using namespace scatha;
using namespace Asm;

void Asm::print(AssemblyStream const& assemblyStream) {
    print(assemblyStream, std::cout);
}

void Asm::print(AssemblyStream const& stream, std::ostream& str) {
    for (auto& block: stream) {
        print(block, str);
    }
}

void Asm::print(Block const& block) { print(block, std::cout); }

void Asm::print(Block const& block, std::ostream& str) {
    std::cout << block.name() << "/" << block.id() << ":\n";
    for (auto& inst: block) {
        str << inst << "\n";
    }
}

static constexpr utl::streammanip instName =
    [](std::ostream& str, auto const&... args) {
    int const instNameWidth = 8;
    str << "  " << std::setw(instNameWidth) << std::left
        << utl::strcat(args...);
};

static void printImpl(std::ostream& str, MoveInst const& mov) {
    str << instName("mov", 8 * mov.numBytes()) << " " << mov.dest() << ", "
        << mov.source();
}

static void printImpl(std::ostream& str, CMoveInst const& cmov) {
    str << instName(toCMoveInstName(cmov.condition()), cmov.numBytes()) << " "
        << cmov.dest() << ", " << cmov.source();
}

static void printImpl(std::ostream& str, UnaryArithmeticInst const& inst) {
    str << instName(inst.operation()) << " " << inst.operand();
}

static void printImpl(std::ostream& str, ArithmeticInst const& inst) {
    str << instName(inst.operation(), 8 * inst.width()) << " " << inst.dest()
        << ", " << inst.source();
}

static void printImpl(std::ostream& str, JumpInst const& jmp) {
    str << instName(toJumpInstName(jmp.condition())) << " "
        << jmp.targetLabelID();
}

static void printImpl(std::ostream& str, CallInst const& call) {
    str << instName("call") << " " << call.functionLabelID() << ", "
        << call.regPtrOffset();
}

static void printImpl(std::ostream& str, CallExtInst const& call) {
    str << instName("callExt") << " " << call.regPtrOffset() << ", "
        << call.slot() << ", " << call.index();
}

static void printImpl(std::ostream& str, ReturnInst const&) {
    str << instName("ret");
}

static void printImpl(std::ostream& str, TerminateInst const&) {
    str << instName("terminate");
}

static void printImpl(std::ostream& str, LIncSPInst const& lincsp) {
    str << instName("lincsp") << " " << lincsp.dest() << ", "
        << lincsp.offset();
}

static void printImpl(std::ostream& str, LEAInst const& lea) {
    str << instName("lea") << " " << lea.dest() << ", " << lea.address();
}

static void printImpl(std::ostream& str, LDAInst const& lda) {
    str << instName("lda") << " " << lda.dest() << ", " << lda.offset();
}

static void printImpl(std::ostream& str, CompareInst const& cmp) {
    // clang-format off
    auto name = UTL_MAP_ENUM(cmp.type(), std::string_view, {
        { Type::Signed,   "scmp" },
        { Type::Unsigned, "ucmp" },
        { Type::Float,    "fcmp" },
    }); // clang-format on
    str << instName(name, cmp.width() * 8) << " " << cmp.lhs() << ", "
        << cmp.rhs();
}

static void printImpl(std::ostream& str, TestInst const& test) {
    str << instName(test.type() == Type::Signed ? "stest" : "utest") << " "
        << test.operand();
}

static void printImpl(std::ostream& str, SetInst const& set) {
    str << instName(toSetInstName(set.operation())) << " " << set.dest();
}

static void printImpl(std::ostream& str, ConvInst const& conv) {
    switch (conv.type()) {
    case Type::Signed:
        str << instName("sext", conv.fromBits()) << " " << conv.operand();
        break;
    case Type::Float:
        if (conv.fromBits() == 32) {
            str << instName("fext") << " " << conv.operand();
        }
        else {
            str << instName("ftrunc") << " " << conv.operand();
        }
        break;
    default:
        SC_UNREACHABLE();
    }
}

std::ostream& Asm::operator<<(std::ostream& str, Instruction const& inst) {
    inst.visit([&](auto& inst) { printImpl(str, inst); });
    return str;
}

namespace {

struct CoutRestore {
    CoutRestore(): flags(std::cout.flags()) {}
    ~CoutRestore() { std::cout.flags(flags); }
    std::ios_base::fmtflags flags;
};

} // namespace

static void printImpl(std::ostream& str, Value8 const& value) {
    CoutRestore r;
    str << std::hex << value.value() << "_u8";
}

static void printImpl(std::ostream& str, Value16 const& value) {
    CoutRestore r;
    str << std::hex << value.value() << "_u16";
}

static void printImpl(std::ostream& str, Value32 const& value) {
    CoutRestore r;
    str << std::hex << value.value() << "_u32";
}

static void printImpl(std::ostream& str, Value64 const& value) {
    CoutRestore r;
    str << std::hex << value.value() << "_u64";
}

static void printImpl(std::ostream& str, RegisterIndex const& regIdx) {
    str << "%" << regIdx.value();
}

static void printImpl(std::ostream& str, MemoryAddress const& addr) {
    str << "*(ptr)%" << addr.baseptrRegisterIndex();
    if (!addr.onlyEvaluatesInnerOffset()) {
        str << " + %" << addr.offsetCountRegisterIndex() << " * "
            << addr.constantOffsetMultiplier();
    }
    str << " + " << addr.constantInnerOffset();
}

std::ostream& Asm::operator<<(std::ostream& str, Value const& value) {
    value.visit([&](auto& value) { printImpl(str, value); });
    return str;
}
