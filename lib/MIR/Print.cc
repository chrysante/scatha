#include "MIR/Print.h"

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

static constexpr auto globalName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::green | tfmt::italic, "@", args...);
    });

static constexpr auto localName =
    utl::streammanip([](std::ostream& str, auto const&... args) -> auto& {
        return str << tfmt::format(tfmt::grey | tfmt::italic, "%", args...);
    });

static constexpr auto regName =
    utl::streammanip([](std::ostream& str, Register const* reg) -> auto& {
        return str << tfmt::format(tfmt::blue, "%", reg->index());
    });

static constexpr auto opcode = keyword;

namespace {

std::string formatInstCode(InstCode code, uint64_t data) {
    switch (code) {
    case InstCode::CondCopy:
        return utl::strcat("ccpy-",
                           toString(static_cast<CompareOperation>(data)));
    case InstCode::Compare:
        return std::string(toString(static_cast<CompareMode>(data)));
    case InstCode::Set:
        return utl::strcat("set-",
                           toString(static_cast<CompareOperation>(data)));
    case InstCode::Test:
        switch (static_cast<CompareMode>(data)) {
        case CompareMode::Signed:
            return "stest";
        case CompareMode::Unsigned:
            return "utest";
        default:
            SC_UNREACHABLE();
        }
    case InstCode::UnaryArithmetic:
        return std::string(
            toString(static_cast<UnaryArithmeticOperation>(data)));
    case InstCode::Arithmetic:
        return std::string(toString(static_cast<ArithmeticOperation>(data)));
    case InstCode::Conversion:
        return std::string(toString(static_cast<Conversion>(data)));
    case InstCode::CondJump:
        return utl::strcat("cjmp-",
                           toString(static_cast<CompareOperation>(data)));
    default:
        break;
    }
    return std::string(mir::toString(code));
}

struct PrintContext {
    PrintContext(std::ostream& str): str(str) {}

    void print(Function const* F) {
        str << keyword("func") << " " << globalName(F->name()) << " { \n";
        indent.increase();
        for (auto& BB: *F) {
            print(&BB);
        }
        indent.decrease();
        str << "}\n";
    }

    void print(BasicBlock const* BB) {
        str << indent << localName(BB->name()) << ": \n";
        indent.increase();
        for (auto& inst: *BB) {
            print(&inst);
        }
        indent.decrease();
    }

    void print(Instruction const* inst) {
        str << indent;
        if (auto* reg = inst->dest()) {
            str << regName(reg) << " = ";
        }
        std::string opcodeStr =
            formatInstCode(inst->instcode(), inst->instData());
        str << opcode(opcodeStr) << " ";
        for (bool first = true; auto* op: inst->operands()) {
            if (!first) {
                str << ", ";
            }
            first = false;
            print(op);
        }
        str << "\n";
    }

    void print(Value const* value) {
        // clang-format off
        visit(*value, utl::overload{
            [&](Register const& reg) {
                str << regName(&reg);
            },
            [&](Constant const& C) {
                str << "0x" << std::hex << C.value() << std::dec;
            },
            [&](BasicBlock const& BB) {
                str << localName(BB.name());
            },
            [&](Function const& F) {
                str << globalName(F.name());
            }
        }); // clang-format on
    }

    std::ostream& str;
    Indenter indent{ 2 };
};

} // namespace

void mir::print(Function const& F, std::ostream& str) {
    PrintContext ctx(str);
    ctx.print(&F);
}
