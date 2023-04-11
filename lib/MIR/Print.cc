#include "MIR/Print.h"

#include <iomanip>
#include <iostream>

#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Basic/PrintUtil.h"
#include "MIR/CFG.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace mir;

void mir::print(Module const& mod) { mir::print(mod, std::cout); }

void mir::print(Module const& mod, std::ostream& str) {
    for (auto& F: mod) {
        print(F, str);
        str << "\n";
    }
}

void mir::print(Function const& F) { mir::print(F, std::cout); }

static constexpr auto keyword =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::magenta | tfmt::bold, args...);
    });

static constexpr auto literal =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::cyan, args...);
    });

static constexpr auto globalName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::green | tfmt::italic, "@", args...);
    });

static constexpr auto localName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::italic, "%", args...);
    });

static constexpr auto regName =
    utl::streammanip([](std::ostream& str, Register const* reg) -> auto& {
        if (reg->isCalleeRegister()) {
            auto name = utl::strcat("%c", reg->index());
            return str << tfmt::format(tfmt::blue | tfmt::italic, name);
        }
        auto name = utl::strcat("%", reg->index());
        return str << tfmt::format(tfmt::brightBlue, name);
    });

static constexpr auto opcode =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::red | tfmt::bold, args...);
    });
;

static constexpr auto light =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::brightGrey | tfmt::italic, args...);
    });

static std::string formatInstCode(mir::Instruction const& inst) {
    switch (inst.instcode()) {
    case InstCode::Copy:
        return utl::strcat("cpy", inst.bitwidth());
    case InstCode::CondCopy:
        return utl::strcat("c",
                           inst.instDataAs<CompareOperation>(),
                           inst.bitwidth());
    case InstCode::Compare:
        return utl::strcat(inst.instDataAs<CompareMode>(), inst.bitwidth());
    case InstCode::Set:
        return utl::strcat("set", inst.instDataAs<CompareOperation>());
    case InstCode::Test:
        switch (inst.instDataAs<CompareMode>()) {
        case CompareMode::Signed:
            return utl::strcat("stest", inst.bitwidth());
        case CompareMode::Unsigned:
            return utl::strcat("utest", inst.bitwidth());
        default:
            SC_UNREACHABLE();
        }
    case InstCode::UnaryArithmetic:
        return utl::strcat(toString(
                               inst.instDataAs<UnaryArithmeticOperation>()),
                           inst.bitwidth());
    case InstCode::Arithmetic:
        return utl::strcat(inst.instDataAs<ArithmeticOperation>(),
                           inst.bitwidth());
    case InstCode::Conversion:
        return utl::strcat(inst.instDataAs<Conversion>());
    case InstCode::CondJump:
        return utl::strcat("j", inst.instDataAs<CompareOperation>());
    default:
        break;
    }
    return std::string(mir::toString(inst.instcode()));
}

namespace {

struct PrintContext {
    PrintContext(std::ostream& str): str(str) {}

    void print(Function const* F) {
        str << keyword("func") << " " << globalName(F->name()) << " {";
        indent.increase();
        for (auto& BB: *F) {
            str << "\n";
            print(&BB);
        }
        indent.decrease();
        str << "}\n";
    }

    void print(BasicBlock const* BB) {
        if (!BB->isEntry()) {
            str << indent << localName(BB->name()) << ": ";
            printPredList(BB);
            str << "\n";
        }
        indent.increase();
        printLiveList("Live in", BB->liveIn());
        for (auto& inst: *BB) {
            print(&inst);
        }
        printLiveList("Live out", BB->liveOut());
        indent.decrease();
    }

    void printPredList(BasicBlock const* BB) {
        str << light("preds: ");
        bool first = true;
        for (auto* pred: BB->predecessors()) {
            str << light(first ? first = false, "" : ", ", pred->name());
        }
    }

    void printLiveList(std::string_view name,
                       utl::hashset<Register*> const& regs) {
        str << indent << light(name, ": [ ");
        bool first = true;
        for (auto* reg: regs) {
            str << light(first ? first = false, "" : ", ") << regName(reg);
        }
        str << light(" ]") << "\n";
    }

    void print(Instruction const* inst) {
        str << indent;
        if (auto* reg = inst->dest()) {
            str << std::setw(3) << std::left << regName(reg) << " = ";
        }
        else {
            str << std::setw(6) << "";
        }
        std::string opcodeStr = formatInstCode(*inst);
        str << std::left << std::setw(6) << opcode(opcodeStr) << " ";
        for (bool first = true; auto* op: inst->operands()) {
            if (!first) {
                str << ", ";
            }
            first = false;
            print(op);
        }
        if (inst->instcode() == InstCode::Load ||
            inst->instcode() == InstCode::Store ||
            inst->instcode() == InstCode::LEA)
        {
            printPtrData(inst->instDataAs<MemoryAddress::ConstantData>());
        }
        str << "\n";
    }

    void print(Value const* value) {
        if (!value) {
            str << keyword("null");
            return;
        }
        // clang-format off
        visit(*value, utl::overload{
            [&](Register const& reg) {
                str << regName(&reg);
            },
            [&](Constant const& C) {
                str << literal("0x", std::hex, C.value(), std::dec);
            },
            [&](UndefValue const&) {
                str << keyword("undef");
            },
            [&](BasicBlock const& BB) {
                str << localName(BB.name());
            },
            [&](Function const& F) {
                str << globalName(F.name());
            }
        }); // clang-format on
    }

    void printPtrData(MemoryAddress::ConstantData data) {
        str << ", of=" << data.offsetFactor << ", ot=" << data.offsetTerm;
    }

    std::ostream& str;
    Indenter indent{ 2 };
};

} // namespace

void mir::print(Function const& F, std::ostream& str) {
    PrintContext ctx(str);
    ctx.print(&F);
}
