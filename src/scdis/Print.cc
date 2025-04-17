#include "scdis/Print.h"

#include <iomanip>
#include <ostream>

#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <termfmt/termfmt.h>
#include <utl/scope_guard.hpp>

#include "scdis/Disassembly.h"

using namespace scdis;
using namespace ranges::views;

static std::string_view prettyInstName(scbinutil::OpCode opcode) {
    auto name = scbinutil::toString(opcode);
    using namespace std::string_view_literals;
    constexpr std::array suffixes = { "8"sv, "16"sv, "32"sv, "64"sv };
    for (auto suffix: suffixes) {
        auto pos = name.find(suffix);
        if (pos != std::string_view::npos)
            return name.substr(0, pos + suffix.size());
    }
    return name;
}

namespace {

struct PrintCtx {
    Disassembly const& disasm;
    PrintDelegate& del;

    void printLabelName(InstructionPointerOffset ipo);
    void printValue(Value value);
    void printInstruction(Instruction inst);
    void printVar(Variable const& var);
    void printAll();
};

} // namespace

void PrintCtx::printLabelName(InstructionPointerOffset ipo) {
    if (auto* label = disasm.findLabel(ipo)) {
        del.labelName(*label);
    }
    else {
        del.plaintext("ipo: ");
        del.immediate(ipo.value);
    }
}

namespace {

struct Addr {
    uint8_t baseRegIdx;
    uint8_t offsetRegIdx;
    uint8_t offsetFactor;
    uint8_t offsetTerm;
};

} // namespace

void PrintCtx::printValue(Value value) {
    using enum ValueType;
    switch (value.type) {
    case RegisterIndex:
        del.registerName(value.raw);
        return;
    case Address: {
        auto addr = std::bit_cast<Addr>(static_cast<uint32_t>(value.raw));
        del.leftBracket();
        del.registerName(addr.baseRegIdx);
        if (addr.offsetRegIdx != 0xFF) {
            del.plus();
            del.immediate(addr.offsetFactor);
            del.star();
            del.registerName(addr.offsetRegIdx);
        }
        if (addr.offsetTerm != 0) {
            del.plus();
            del.immediate(addr.offsetTerm);
        }
        del.rightBracket();
        return;
    }
    case Value8:
    case Value16:
    case Value32:
    case Value64:
        del.immediate(value.raw);
        return;
    }
}

void PrintCtx::printInstruction(Instruction inst) {
    del.beginInst(inst);
    utl::scope_guard end = [&] { del.endInst(); };
    del.instName(prettyInstName(inst.opcode));
    using enum scbinutil::OpCodeClass;
    switch (scbinutil::classify(inst.opcode)) {
    case RR:
    case RV64:
    case RV32:
    case RV8:
    case RM:
    case MR:
        del.space();
        printValue(inst.arg1);
        del.comma();
        printValue(inst.arg2);
        return;
    case R:
        del.space();
        printValue(inst.arg1);
        return;
    case Jump:
        del.space();
        printLabelName(InstructionPointerOffset(inst.arg1.raw));
        return;
    case Other:
        using enum scbinutil::OpCode;
        switch (inst.opcode) {
        case lincsp:
            del.space();
            printValue(inst.arg1);
            del.comma();
            printValue(inst.arg2);
            return;
        case call:
            del.space();
            printLabelName(InstructionPointerOffset(inst.arg1.raw));
            del.comma();
            printValue(inst.arg2);
            return;
        case icallr:
            del.space();
            printValue(inst.arg1);
            del.comma();
            printValue(inst.arg2);
            return;
        case icallm:
            return;
        case ret:
            return;
        case terminate:
            return;
        case trap:
            return;
        case cfng:
        case cbltn: {
            del.space();
            printValue(inst.arg1);
            del.comma();
            if (inst.opcode == cfng) {
                if (auto* fn = disasm.findFfiByIndex(inst.arg2.raw)) {
                    del.ffiName(fn->name);
                    return;
                }
            }
            if (inst.opcode == cbltn) {
                del.ffiName(svm::toString((svm::Builtin)inst.arg2.raw));
                return;
            }
            del.plaintext("index: ");
            del.immediate(inst.arg2.raw);
            return;
        }
        default:
            del.plaintext("Unknown Instruction");
            return;
        }
    }
}

void PrintCtx::printVar(Variable const& var) {
    del.label(var.label);
    del.beginData(var.ipo);
    del.string({ (char const*)var.data.data(), var.data.size() });
    del.endData();
}

void PrintCtx::printAll() {
    for (auto& var: disasm.variables())
        printVar(var);
    for (auto [index, inst]: disasm.instructions() | enumerate) {
        if (auto* label = disasm.findLabel(index)) del.label(*label);
        printInstruction(inst);
    }
}

void scdis::print(Disassembly const& disasm, PrintDelegate& delegate) {
    PrintCtx{ disasm, delegate }.printAll();
}

namespace {

template <bool UseColor>
struct OstreamDelegate final: PrintDelegate {
    std::ostream& os;

    explicit OstreamDelegate(std::ostream& os): os(os) {}

    void instName(std::string_view name) override {
        format(tfmt::BrightBlue, std::setw(9), std::left, name);
    }

    void registerName(size_t index) override {
        format(tfmt::Magenta, "%", index);
    }

    void immediate(uint64_t value) override { format(tfmt::Cyan, value); }

    static tfmt::Modifier const& LabelMod(Label::Type type) {
        switch (type) {
        case Label::Function: {
            static auto const mod = tfmt::Green | tfmt::Bold;
            return mod;
        }
        case Label::Block:
        case Label::String:
        case Label::Raw:
            static auto const mod = tfmt::Bold;
            return mod;
        }
    }

    void label(Label const& label) override {
        format(LabelMod(label.type), label.name, ":\n");
    }

    void labelName(Label const& label) override {
        format(LabelMod(label.type), label.name);
    }

    void ffiName(std::string_view name) override { format(tfmt::Yellow, name); }

    void string(std::string_view value) override {
        format(tfmt::Red, "\"", value, "\"");
    }

    void plaintext(std::string_view text) override { os << text; }

    void beginLine(InstructionPointerOffset ipo) const {
        format(tfmt::BrightGrey, std::setw(5), std::right, ipo.value, ": ");
    }

    void beginInst(Instruction const& inst) override { beginLine(inst.ipo); }

    void endInst() override { os << "\n"; }

    void beginData(InstructionPointerOffset ipo) override { beginLine(ipo); }

    void endData() override { os << "\n"; }

    void format(tfmt::Modifier const& mod, auto const&... args) const {
        if constexpr (UseColor)
            os << tfmt::format(mod, args...);
        else
            ((os << args), ...);
    }
};

} // namespace

void scdis::print(Disassembly const& disasm, std::ostream& ostream,
                  bool useColor) {
    if (useColor) {
        OstreamDelegate<true> delegate(ostream);
        print(disasm, delegate);
    }
    else {
        OstreamDelegate<false> delegate(ostream);
        print(disasm, delegate);
    }
}
