#include "Assembly/Print.h"

#include <iomanip>
#include <iostream>

#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Assembly/AssemblyStream.h"
#include "Assembly/Block.h"
#include "Assembly/Instruction.h"
#include "Assembly/Value.h"

using namespace scatha;
using namespace Asm;

static constexpr utl::streammanip instName = [](std::ostream& str,
                                                auto const&... args) {
    int const instNameWidth = 8;
    str << "  " << std::setw(instNameWidth) << std::left
        << utl::strcat(args...);
};

static std::string_view typeToChar(Type type) {
    switch (type) {
    case Type::Signed:
        return "s";
    case Type::Unsigned:
        return "u";
    case Type::Float:
        return "f";
    case Type::_count:
        SC_UNREACHABLE();
    }
    SC_UNREACHABLE();
}

namespace {

struct OStreamRestore {
    explicit OStreamRestore(std::ostream& str): str(str), flags(str.flags()) {}
    ~OStreamRestore() { str.flags(flags); }

    std::ostream& str;
    std::ios_base::fmtflags flags;
};

struct PrintCtx {
    std::ostream& str;
    utl::hashmap<LabelID, Block const*> blockIDMap;

    /// Prints the name of the target block if it exists in the map, otherwise
    /// prints the ID
    auto label(LabelID ID) {
        return utl::streammanip([this, ID](std::ostream& str) {
            auto itr = blockIDMap.find(ID);
            if (itr != blockIDMap.end()) {
                auto* block = itr->second;
                str << block->name();
            }
            else {
                str << "Label: " << utl::to_underlying(ID);
            }
        });
    }

    explicit PrintCtx(std::ostream& str): str(str) {}

    void print(AssemblyStream const& stream) {
        for (auto& block: stream) {
            blockIDMap[block.id()] = &block;
        }
        for (auto& block: stream) {
            print(block);
        }
    }


    void print(Block const& block) {
        str << block.name() << ": ID: " << utl::to_underlying(block.id())
            << "\n";
        for (auto& inst: block) {
            print(inst);
            str << "\n";
        }
    }

    void print(Instruction const& inst) {
        std::visit([&](auto& inst) { printImpl(inst); }, inst);
    }

    void print(Value const& value) {
        std::visit([&](auto& value) { printImpl(value); }, value);
    }

    void printImpl(MoveInst const& mov) {
        str << instName("mov", 8 * mov.numBytes()) << " " << mov.dest() << ", "
            << mov.source();
    }

    void printImpl(CMoveInst const& cmov) {
        str << instName(toCMoveInstName(cmov.condition()), cmov.numBytes())
            << " " << cmov.dest() << ", " << cmov.source();
    }

    void printImpl(UnaryArithmeticInst const& inst) {
        str << instName(inst.operation()) << " " << inst.operand();
    }

    void printImpl(ArithmeticInst const& inst) {
        str << instName(inst.operation(), 8 * inst.width()) << " "
            << inst.dest() << ", " << inst.source();
    }

    void printImpl(JumpInst const& jmp) {
        str << instName(toJumpInstName(jmp.condition())) << " "
            << label(jmp.target());
    }

    void printImpl(CallInst const& call) {
        str << instName("call") << " " << call.dest() << ", "
            << call.regPtrOffset();
    }

    void printImpl(CallExtInst const& call) {
        str << instName("callExt") << " " << call.regPtrOffset() << ", "
            << call.callee();
    }

    void printImpl(ReturnInst const&) { str << instName("ret"); }

    void printImpl(TerminateInst const&) { str << instName("terminate"); }

    void printImpl(LIncSPInst const& lincsp) {
        str << instName("lincsp") << " " << lincsp.dest() << ", "
            << lincsp.offset();
    }

    void printImpl(LEAInst const& lea) {
        str << instName("lea") << " " << lea.dest() << ", " << lea.address();
    }

    void printImpl(CompareInst const& cmp) {
        // clang-format off
        auto name = UTL_MAP_ENUM(cmp.type(), std::string_view, {
            { Type::Signed,   "scmp" },
            { Type::Unsigned, "ucmp" },
            { Type::Float,    "fcmp" },
        }); // clang-format on
        str << instName(name, cmp.width() * 8) << " " << cmp.lhs() << ", "
            << cmp.rhs();
    }

    void printImpl(TestInst const& test) {
        str << instName(test.type() == Type::Signed ? "stest" : "utest") << " "
            << test.operand();
    }

    void printImpl(SetInst const& set) {
        str << instName(toSetInstName(set.operation())) << " " << set.dest();
    }

    void printImpl(TruncExtInst const& conv) {
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

    void printImpl(ConvertInst const& conv) {
        str << instName(typeToChar(conv.fromType()),
                        conv.fromBits(),
                        "to",
                        typeToChar(conv.toType()),
                        conv.toBits())
            << " " << conv.operand();
    }

    void printImpl(Value8 const& value) {
        OStreamRestore r(str);
        str << "0x" << std::hex << value.value() << "_u8";
    }

    void printImpl(Value16 const& value) {
        OStreamRestore r(str);
        str << "0x" << std::hex << value.value() << "_u16";
    }

    void printImpl(Value32 const& value) {
        OStreamRestore r(str);
        str << "0x" << std::hex << value.value() << "_u32";
    }

    void printImpl(Value64 const& value) {
        OStreamRestore r(str);
        str << "0x" << std::hex << value.value() << "_u64";
    }

    void printImpl(LabelPosition const& pos) {
        OStreamRestore r(str);
        str << label(pos.ID());
    }

    void printImpl(RegisterIndex const& regIdx) {
        str << "%" << regIdx.value();
    }

    void printImpl(MemoryAddress const& addr) {
        str << "[ %" << addr.baseptrRegisterIndex();
        if (!addr.onlyEvaluatesInnerOffset()) {
            str << " + %" << addr.offsetCountRegisterIndex() << " * "
                << addr.constantOffsetMultiplier();
        }
        if (addr.constantInnerOffset() > 0) {
            str << " + " << addr.constantInnerOffset();
        }
        str << " ]";
    }
};

} // namespace

void Asm::print(AssemblyStream const& assemblyStream) {
    print(assemblyStream, std::cout);
}

void Asm::print(AssemblyStream const& stream, std::ostream& str) {
    PrintCtx(str).print(stream);
}

void Asm::print(Block const& block) { print(block, std::cout); }

void Asm::print(Block const& block, std::ostream& str) {
    PrintCtx(str).print(block);
}

void Asm::print(Instruction const& inst) { print(inst, std::cout); }

void Asm::print(Instruction const& inst, std::ostream& str) {
    PrintCtx(str).print(inst);
    str << "\n";
}

std::ostream& Asm::operator<<(std::ostream& str, Instruction const& inst) {
    PrintCtx(str).print(inst);
    return str;
}

void Asm::print(Value const& value) { print(value, std::cout); }

void Asm::print(Value const& value, std::ostream& str) {
    PrintCtx(str).print(value);
    str << "\n";
}

std::ostream& Asm::operator<<(std::ostream& str, Value const& value) {
    PrintCtx(str).print(value);
    return str;
}
