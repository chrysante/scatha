#include "IC/PrintTac.h"

#include <iostream>
#include <ostream>

namespace scatha::ic {

void printTac(ThreeAddressCode const& tac, sema::SymbolTable const& sym) {
    printTac(tac, sym, std::cout);
}

namespace {

void printLabel(std::ostream& str, Label const& label, sema::SymbolTable const& sym) {
    str << sym.getFunction(label.functionID).name();
    if (label.index >= 0) {
        str << ".L" << label.index;
    }
}

struct ArgumentPrinter {
    ArgumentPrinter(TasArgument const& arg, sema::SymbolTable const& sym): arg(arg), sym(sym) {}

    friend std::ostream& operator<<(std::ostream& str, ArgumentPrinter const& p) { return p.print(str); }

    std::ostream& print(std::ostream& str) const {
                                       return arg.visit(utl::visitor{
					[&](EmptyArgument const&) -> auto& {
						return str << "<empty-argument>";
    }
    , [&](Variable const& var) -> auto& {
        for (bool first = true; auto id : var) {
            str << (first ? ((void)(first = false), "$") : ".") << sym.getName(id);
        }
        return str;
        //						return str << "$" <<
        // sym.getVariable(var.id()).name();
    }
    , [&](Temporary const& tmp) -> auto& { return str << "T[" << tmp.index << "]"; }
    , [&](LiteralValue const& lit) -> auto& {
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
    , [&](Label const& label) -> auto& {
        printLabel(str, label, sym);
        return str;
    }
    , [&](FunctionLabel const& label) -> auto& { return str << sym.getFunction(label.functionID()).name(); }
    , [&](If) -> auto& {
        SC_DEBUGFAIL();
        return str;
    }
                                });
                                } // namespace

                                TasArgument arg;
                                sema::SymbolTable const& sym;
                                }; // namespace scatha::ic

                                ArgumentPrinter print(TasArgument const& arg, sema::SymbolTable const& sym) {
                                    return ArgumentPrinter(arg, sym);
                                }

                                } // namespace

                                void printTac(ThreeAddressCode const& tac,
                                              sema::SymbolTable const& sym,
                                              std::ostream& str) {
                                    for (auto const& line : tac.statements) {
                                        std::visit(utl::visitor{ [&](Label const& label) {
                                                                    printLabel(str, label, sym);
                                                                    str << ":\n";
                                                                },
                                                                 [&](FunctionLabel const& label) {
            auto const& function = sym.getFunction(label.functionID());
            str << function.name() << ":\n";
                                                                },
                                                                 [&](FunctionEndLabel) { str << "FUNCTION_END\n"; },
                                                                 [&](ThreeAddressStatement const& s) {
            str << "    ";

            if (s.result.is(TasArgument::conditional)) {
                if (s.operation == Operation::ifPlaceholder) {
                    str << "if " << print(s.arg1, sym) << "\n";
                }
                else {
                    SC_ASSERT(isRelop(s.operation),
                              "Must be "
                              "relop");
                    str << "if " << s.operation << " " << print(s.arg1, sym) << ", " << print(s.arg2, sym) << "\n";
                }
                return;
            }
            else {
                if (!s.result.is(TasArgument::empty)) {
                    str << print(s.result, sym) << " = ";
                }
                str << s.operation;
            }

            int const argCount = argumentCount(s.operation);
            switch (argCount) {
            case 0: break;
            case 1: str << " " << print(s.arg1, sym); break;
            case 2:
                str << " " << print(s.arg1, sym) << ", " << print(s.arg2, sym);
                break;

                SC_NO_DEFAULT_CASE();
            }
            str << "\n";
                                        } },
                                            line);
                                    }
                                }
                                }
