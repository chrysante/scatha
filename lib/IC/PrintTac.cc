#include "IC/PrintTac.h"

#include <iostream>

using namespace scatha;
using namespace ic;

namespace {

void printLabel(std::ostream& str, Label const& label, sema::SymbolTable const& sym) {
    str << sym.getFunction(label.functionID).name();
    if (label.index >= 0) {
        str << ".L" << label.index;
    }
}

struct ArgumentPrinter {
    ArgumentPrinter(TasArgument const& arg, sema::SymbolTable const& sym): arg(arg), sym(sym) {}

    friend std::ostream& operator<<(std::ostream& str, ArgumentPrinter p) {
        p._str = &str;
        return p.dispatch();
    }

    std::ostream& dispatch() const {
        return arg.visit([this](auto& elem) -> auto& { return print(elem); });
    }

    std::ostream& print(EmptyArgument const&) const {
        std::ostream& str = *_str;
        return str << "<empty-argument>";
    }

    std::ostream& print(Variable const& var) const {
        std::ostream& str = *_str;
        for (bool first = true; auto id: var) {
            str << (first ? ((void)(first = false), "$") : ".") << sym.getName(id);
        }
        return str;
    }

    std::ostream& print(Temporary const& tmp) const {
        std::ostream& str = *_str;
        return str << "T[" << tmp.index << "]";
    }

    std::ostream& print(LiteralValue const& lit) const {
        std::ostream& str = *_str;
        if (lit.type == sym.Bool()) {
            return str << (bool(lit.value) ? "true" : "false");
        }
        else if (lit.type == sym.Int()) {
            return str << static_cast<i64>(lit.value);
        }
        else if (lit.type == sym.Float()) {
            return str << utl::bit_cast<f64>(lit.value);
        }
        else {
            return str << lit.value << " [Type = " << sym.getName(lit.type) << "]";
        }
    }

    std::ostream& print(Label const& label) const {
        std::ostream& str = *_str;
        printLabel(str, label, sym);
        return str;
    }

    std::ostream& print(FunctionLabel const& label) const {
        std::ostream& str = *_str;
        return str << sym.getFunction(label.functionID()).name();
    }

    std::ostream& print(If const&) const {
        [[maybe_unused]] std::ostream& str = *_str;
        SC_DEBUGFAIL();
    }

    std::ostream* _str;
    TasArgument arg;
    sema::SymbolTable const& sym;
};

ArgumentPrinter print(TasArgument const& arg, sema::SymbolTable const& sym) {
    return ArgumentPrinter(arg, sym);
}

struct Context {
    void run();
    void dispatch(TacLine const&);
    void print(Label const&);
    void print(FunctionLabel const&);
    void print(FunctionEndLabel const&);
    void print(ThreeAddressStatement const&);

    ThreeAddressCode const& tac;
    sema::SymbolTable const& sym;
    std::ostream& str;
};

} // namespace

void ic::printTac(ThreeAddressCode const& tac, sema::SymbolTable const& sym) {
    ic::printTac(tac, sym, std::cout);
}

void ic::printTac(ThreeAddressCode const& tac, sema::SymbolTable const& sym, std::ostream& str) {
    Context ctx{ tac, sym, str };
    ctx.run();
}

void Context::run() {
    for (auto const& line: tac.statements) {
        dispatch(line);
    }
}

void Context::dispatch(TacLine const& line) {
    std::visit([this](auto& elem) { print(elem); }, line);
}

void Context::print(Label const& label) {
    printLabel(str, label, sym);
    str << ":\n";
}

void Context::print(FunctionLabel const& label) {
    auto const& function = sym.getFunction(label.functionID());
    str << function.name() << ":\n";
}

void Context::print(FunctionEndLabel const&) {
    str << "FUNCTION_END\n";
}

void Context::print(ThreeAddressStatement const& s) {
    str << "    ";
    if (s.result.is(TasArgument::conditional)) {
        if (s.operation == Operation::ifPlaceholder) {
            str << "if " << ::print(s.arg1, sym) << "\n";
        }
        else {
            SC_ASSERT(isRelop(s.operation), "Must be a relop");
            str << "if " << s.operation << " " << ::print(s.arg1, sym) << ", " << ::print(s.arg2, sym) << "\n";
        }
        return;
    }
    else {
        if (!s.result.is(TasArgument::empty)) {
            str << ::print(s.result, sym) << " = ";
        }
        str << s.operation;
    }
    switch (argumentCount(s.operation)) {
    case 0: break;
    case 1: str << " " << ::print(s.arg1, sym); break;
    case 2: str << " " << ::print(s.arg1, sym) << ", " << ::print(s.arg2, sym); break;
    default: SC_UNREACHABLE();
    }
    str << "\n";
}
