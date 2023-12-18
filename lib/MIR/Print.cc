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
using namespace ranges::views;

static void print(ForeignFunctionDecl const& extFn, std::ostream& str);

void mir::print(Module const& mod) { mir::print(mod, std::cout); }

void mir::print(Module const& mod, std::ostream& str) {
    for (auto& F: mod.foreignFunctions()) {
        ::print(F, str);
        str << "\n";
    }
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

static constexpr auto fmtIndex =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::BrightGrey,
                                   std::setw(2),
                                   std::right,
                                   args...,
                                   ": ");
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

static constexpr auto fmtExtFnAddr =
    utl::streammanip([](std::ostream& str, ForeignFuncAddress addr) -> auto& {
        return str << "slot=" << addr.slot << ", index=" << addr.index;
    });

static void print(ForeignFunctionDecl const& F, std::ostream& str) {
    str << keyword("func") << " " << globalName(F.name) << "(";
    for (bool first = true; size_t size: F.argTypes) {
        if (!first) {
            str << ", ";
        }
        first = false;
        str << keyword(size);
    }
    str << ") -> ";
    if (F.retType == 0) {
        str << keyword("void");
    }
    else {
        str << keyword(F.retType);
    }
    str << " " << tfmt::format(BrightGrey, "[", fmtExtFnAddr(F.address), "]");
    str << "\n";
}

namespace {

enum LiveState {
    LS_None = 0,
    LS_Begin = 1 << 0,
    LS_Mid = 1 << 1,
    LS_End = 1 << 2,
    LS_BeginEnd = LS_Begin | LS_End,
};

/// Maps program points to live states for a given register
struct RegLiveStateList {
    Register const* reg;
    /// The second int is a bitfield of LiveState
    utl::hashmap<int, int> states;
};

struct PrintContext {
    PrintContext(Function const* F, std::ostream& str): F(F), str(str) {
        computeLiveStates();
    }

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

    void printIndex(ProgramPoint const& PP) {
        if (PP.hasIndex()) {
            str << fmtIndex(PP.index());
        }
        else {
            str << indent;
        }
    }

    void print(BasicBlock const* BB) {
        if (BB->hasIndex()) {
            printLiveStateHeaders(BB);
        }
        printIndex(*BB);
        str << localName(BB->name()) << ": ";
        printPredList(BB);
        str << "\n";
        indent.increase();
        if (BB->hasIndex()) {
            printLiveStates(BB, BB->index());
        }
        str << indent << formatLiveList("Live in", BB->liveIn()) << "\n";
        for (auto& inst: *BB) {
            print(inst);
        }
        if (!BB->empty() && BB->back().hasIndex()) {
            printLiveStates(BB, BB->back().index() + 1);
        }
        str << indent << formatLiveList("Live out", BB->liveOut()) << "\n";
        indent.decrease();
    }

    void printPredList(BasicBlock const* BB) {
        if (BB->predecessors().empty()) {
            return;
        }
        str << light("preds: ");
        bool first = true;
        for (auto* pred: BB->predecessors()) {
            auto predName = pred ? pred->name() : "NULL";
            str << light(first ? first = false, "" : ", ", predName);
        }
    }

    utl::vstreammanip<> formatLiveList(std::string_view name,
                                       utl::hashset<Register*> const& regs) {
        return [name, &regs](std::ostream& str) {
            str << light(name, ": [");
            bool first = true;
            for (auto* reg: regs) {
                str << light(first ? first = false, "" : ", ") << regName(reg);
            }
            str << light("]");
        };
    }

    void print(Instruction const& inst) {
        visit(inst, [&](auto& inst) { printImpl(inst); });
        str << "\n";
    }

    /// Here we print `dest_reg = ` or nothing if the instruction does not write
    /// to a register
    void printInstBegin(Instruction const& inst) {
        if (inst.hasIndex()) {
            printLiveStates(inst.parent(), inst.index());
        }
        printIndex(inst);
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

    void print(ForeignFuncAddress function) {
        if (function.slot == svm::BuiltinFunctionSlot) {
            str << "builtin."
                << toString(static_cast<svm::Builtin>(function.index));
        }
        else {
            str << fmtExtFnAddr(function);
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
        str << tfmt::format(BrightGrey,
                            " [regoffset=",
                            call.registerOffset(),
                            "]");
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
        SC_MATCH (*value) {
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
            [&](Callable const& F) {
                str << globalName(F.name());
            }
        }; // clang-format on
    }

    void printPtrData(MemAddrConstantData data) {
        str << ", of=" << data.offsetFactor << ", ot=" << data.offsetTerm;
    }

    void computeLiveStates() {
        if (!F) {
            return;
        }
        for (auto& BB: *F) {
            if (!BB.hasIndex() || BB.empty() || !BB.back().hasIndex()) {
                continue;
            }
            auto& liveRegs = BBLiveRegs[&BB];
            LiveInterval BBInterval = { BB.index(), BB.back().index() + 1 };
            for (auto& reg: F->allRegisters()) {
                auto liveRange = reg.liveRange();
                auto overlap = rangeOverlap(liveRange, BBInterval);
                if (overlap.empty()) {
                    continue;
                }
                liveRegs.push_back({ .reg = &reg, .states = {} });
                addLiveState(liveRegs.back(), overlap, BB.index());
                addLiveState(liveRegs.back(), overlap, BB.back().index() + 1);
                for (auto& inst: BB | filter(&Instruction::hasIndex)) {
                    addLiveState(liveRegs.back(), overlap, inst.index());
                }
            }
        }
    }

    static int getLiveState(std::span<LiveInterval const> range, int P) {
        int LS = LS_None;
        for (auto I: range) {
            if (I.begin == P) {
                LS |= LS_Begin;
            }
            if (I.end == P) {
                LS |= LS_End;
            }
            if (I.begin < P && P < I.end) {
                LS |= LS_Mid;
            }
        }
        return LS;
    }

    void addLiveState(RegLiveStateList& liveList,
                      std::span<LiveInterval const> range,
                      int P) {
        auto state = getLiveState(range, P);
        liveList.states[P] = state;
        if (state & LS_BeginEnd) {
            liveChanges.insert(P);
        }
    }

    void printLiveStateHeaders(BasicBlock const* BB) {
        auto& liveRegs = BBLiveRegs[BB];
        if (liveRegs.empty()) {
            return;
        }
        for (auto& list: liveRegs) {
            print(list.reg);
            if (list.reg->index() < 10) {
                str << " ";
            }
        }
    }

    void printLiveStates(BasicBlock const* BB, int P) {
        auto& liveRegs = BBLiveRegs[BB];
        if (liveRegs.empty()) {
            return;
        }
        tfmt::FormatGuard fmt(BrightBlue, str);
        for (auto& list: liveRegs) {
            switch (list.states[P]) {
            case LS_Begin:
                str << tfmt::format(Bold, " ┬ ");
                break;
            case LS_Mid:
                str << tfmt::format(BrightGrey, "─");
                str << tfmt::format(Bold, "│");
                str << tfmt::format(BrightGrey, "─");
                break;
            case LS_End:
                str << tfmt::format(Bold, " ┴ ");
                break;
            case LS_BeginEnd:
                if (BB->hasIndex() && P == BB->index()) {
                    str << tfmt::format(Bold, " ┬ ");
                }
                else if (BB->back().hasIndex() && BB->back().index() + 1 == P) {
                    str << tfmt::format(Bold, " ┴ ");
                }
                else {
                    str << tfmt::format(Bold, " ┼ ");
                }
                break;
            default:
                str << tfmt::format(BrightGrey, "───");
                break;
            }
        }
    }

    Function const* F;
    std::ostream& str;
    Indenter indent{ 2 };
    /// Maps program points to live states
    utl::hashmap<BasicBlock const*, std::vector<RegLiveStateList>> BBLiveRegs;
    utl::hashset<int> liveChanges;
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

std::ostream& mir::operator<<(std::ostream& ostream, mir::Value const& value) {
    printDecl(value, ostream);
    return ostream;
}
