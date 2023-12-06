#include "MIR/Print.h"

#include <iomanip>
#include <iostream>

#include <svm/Builtin.h>
#include <termfmt/termfmt.h>
#include <utl/strcat.hpp>
#include <utl/streammanip.hpp>

#include "Common/PrintUtil.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace mir;
using namespace tfmt::modifiers;

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
        return str << tfmt::format(tfmt::Magenta | tfmt::Bold, args...);
    });

static constexpr auto literal =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::Cyan, args...);
    });

static constexpr auto globalName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::Green | tfmt::Italic, "@", args...);
    });

static constexpr auto localName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::Italic, "%", args...);
    });

static constexpr auto regName =
    utl::streammanip([](std::ostream& str, Register const* reg) -> auto& {
        // clang-format off
        char const prefix = visit(*reg, utl::overload{
            [](SSARegister const&) { return 'S'; },
            [](VirtualRegister const&) { return 'V'; },
            [](CalleeRegister const&) { return 'C'; },
            [](HardwareRegister const&) { return 'H'; },
        }); // clang-format on
        auto name = utl::strcat(prefix, reg->index());
        auto fmt = tfmt::BrightBlue;
        if (reg->fixed()) {
            fmt |= tfmt::Bold;
        }
        return str << tfmt::format(fmt, name);
    });

static constexpr auto opcode =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << std::left << std::setw(6)
                   << tfmt::format(tfmt::Red | tfmt::Bold, args...);
    });

static constexpr auto light =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::BrightGrey | tfmt::Italic, args...);
    });

static constexpr auto none =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::None, args...);
    });

static auto formatInstName(mir::Instruction const& inst) {
    using namespace std::literals;
    // clang-format off
    std::string name = SC_MATCH (inst) {
        [](StoreInst const& inst) {
            return utl::strcat("store", inst.bitwidth());
        },
        [](LoadInst const& inst) {
            return utl::strcat("load", inst.bitwidth());
        },
        [](CopyInst const& inst) {
            return utl::strcat("cpy", inst.bitwidth());
        },
        [](CallInst const& inst) {
            return "call"s;
        },
        [](CallExtInst const& inst) {
            return "callext"s;
        },
        [](CondCopyInst const& inst) {
            return utl::strcat("c",
                               inst.condition(),
                               inst.bitwidth());
        },
        [](LISPInst const& inst) {
            return "lisp"s;
        },
        [](LEAInst const& inst) {
            return "lea"s;
        },
        [](CompareInst const& inst) {
            return utl::strcat(inst.mode(), inst.bitwidth());
        },
        [](SetInst const& inst) {
            return utl::strcat("set", inst.operation());
        },
        [](TestInst const& inst) {
            switch (inst.mode()) {
            case CompareMode::Signed:
                return utl::strcat("stest", inst.bitwidth());
            case CompareMode::Unsigned:
                return utl::strcat("utest", inst.bitwidth());
            default:
                SC_UNREACHABLE();
            }
        },
        [](UnaryArithmeticInst const& inst) {
            return utl::strcat(inst.operation(),
                               inst.bitwidth());
        },
        [](ArithmeticInst const& inst) {
            return utl::strcat(inst.operation(),
                               inst.bitwidth());
        },
        [](ConversionInst const& inst) {
            return utl::strcat(inst.conversion());
        },
        [](JumpInst const& inst) {
            return "jmp"s;
        },
        [](CondJumpInst const& inst) {
            return utl::strcat("j", inst.condition());
        },
        [](Instruction const& inst) {
            return std::string(mir::toString(inst.instType()));
        },
        [](ReturnInst const& inst) {
            return "ret"s;
        },
        [](PhiInst const& inst) {
            return "phi"s;
        },
        [](SelectInst const& inst) {
            return "select"s;
        },
    }; // clang-format on
    return opcode(name);
}

namespace {

struct PrintContext {
    PrintContext(Function const* F, std::ostream& str): F(F), str(str) {}

    void print() {
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
            print(inst);
        }
        printLiveList("Live out", BB->liveOut());
        indent.decrease();
    }

    void printPredList(BasicBlock const* BB) {
        str << light("preds: ");
        bool first = true;
        for (auto* pred: BB->predecessors()) {
            auto predName = pred ? pred->name() : "NULL";
            str << light(first ? first = false, "" : ", ", predName);
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

    void print(Instruction const& inst) {
        visit(inst, [&](auto& inst) { printImpl(inst); });
        str << "\n";
    }

    /// Here we print `dest_reg = ` or nothing if the instruction does not write
    /// to a register
    void printInstBegin(Instruction const& inst) {
        str << indent;
        if (auto* reg = inst.dest()) {
            str << std::setw(3) << std::left << regName(reg) << none(" = ");
            return;
        }
        if (tfmt::isHTMLFormattable(str)) {
            return;
        }
        str << std::setw(6) << "";
    }

    void printOperands(Instruction const& inst) {
        for (bool first = true; auto* op: inst.operands()) {
            if (!first) {
                str << none(", ");
            }
            first = false;
            print(op);
        }
    }

    void print(ConstMemoryAddress addr) {
        str << "[";
        print(addr.baseAddress());
        if (addr.dynOffset()) {
            str << " + " << addr.offsetFactor() << " * ";
            print(addr.dynOffset());
        }
        if (addr.offsetTerm() != 0) {
            str << " + " << addr.offsetTerm();
        }
        str << "]";
    }

    void printImpl(Instruction const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        printOperands(inst);
    }

    void printImpl(StoreInst const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        print(inst.address());
        str << none(", ");
        print(inst.source());
    }

    void printImpl(LoadInst const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        print(inst.address());
    }

    void printImpl(LEAInst const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        print(inst.address());
    }

    void print(ExtFuncAddress function) {
        if (function.slot == svm::BuiltinFunctionSlot) {
            str << "builtin."
                << toString(static_cast<svm::Builtin>(function.index));
        }
        else {
            str << "slot=" << function.slot;
            str << ", index=" << function.index;
        }
    }

    void printImpl(CallBase const& call) {
        printInstBegin(call);
        str << formatInstName(call) << " ";
        if (auto* callext = dyncast<CallExtInst const*>(&call)) {
            print(callext->callee());
            str << none(", ");
        }
        printOperands(call);
        str << " [regoffset=" << call.registerOffset() << "]";
    }

    void printImpl(LoadArithmeticInst const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        print(inst.LHS());
        str << none(", ");
        print(inst.RHS());
    }

    void printImpl(PhiInst const& inst) {
        printInstBegin(inst);
        str << formatInstName(inst) << " ";
        /// For phi instructions we print the operands with the predecessors
        utl::streammanip predName = [&](std::ostream& str, size_t index) {
            auto* parent = inst.parent();
            if (!parent) {
                str << "null";
                return;
            }
            str << localName(parent->predecessors()[index]->name());
        };
        for (auto [index, op]: inst.operands() | ranges::views::enumerate) {
            if (index != 0) {
                str << none(", ");
            }
            str << "[" << predName(index) << ": ";
            print(op);
            str << "]";
        }
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

    void printPtrData(MemAddrConstantData data) {
        str << ", of=" << data.offsetFactor << ", ot=" << data.offsetTerm;
    }

    Function const* F;
    std::ostream& str;
    Indenter indent{ 2 };
};

} // namespace

void mir::print(Function const& F, std::ostream& str) {
    PrintContext ctx(&F, str);
    ctx.print();
}

void mir::print(mir::Instruction const& inst) { mir::print(inst, std::cout); }

void mir::print(mir::Instruction const& inst, std::ostream& str) {
    PrintContext ctx(nullptr, str);
    ctx.print(inst);
}

void mir::printDecl(mir::Value const& value) { printDecl(value, std::cout); }

void mir::printDecl(mir::Value const& value, std::ostream& str) {
    PrintContext ctx(nullptr, str);
    ctx.print(&value);
}
