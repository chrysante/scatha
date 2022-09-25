#include "IC/PrintTac.h"

#include <ostream>
#include <iostream>

namespace scatha::ic {
	
	void printTac(ThreeAddressCode const& tac, sema::SymbolTable const& sym) {
		printTac(tac, sym, std::cout);
	}
	
	namespace {
		
		struct ArgumentPrinter {
			ArgumentPrinter(TasArgument const& arg, sema::SymbolTable const& sym): arg(arg), sym(sym) {}
			
			friend std::ostream& operator<<(std::ostream& str, ArgumentPrinter const& p) {
				return p.print(str);
			}
			
			std::ostream& print(std::ostream& str) const {
				return arg.visit(utl::visitor{
					[&](EmptyArgument const&) -> auto& {
						SC_DEBUGBREAK();
						return str;
					},
					[&](Variable const& var) -> auto& {
						return str << "$" << sym.getVariable(var.id()).name();
					},
					[&](Temporary const& tmp) -> auto& {
						return str << "T[" << tmp.index << "]";
					},
					[&](LiteralValue const& lit) -> auto& {
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
							SC_DEBUGFAIL();
						}
					},
					[&](Label const& label) -> auto& {
						str << sym.getFunction(label.functionID).name();
						if (label.index >= 0) {
							str << ".L" << label.index;
						}
						return str;
					},
					[&](FunctionLabel const& label) -> auto& {
						return str << sym.getFunction(label.functionID()).name();
					}
				});
			}
			
			TasArgument arg;
			sema::SymbolTable const& sym;
		};
		
		ArgumentPrinter print(TasArgument const& arg, sema::SymbolTable const& sym) {
			return ArgumentPrinter(arg, sym);
		}
		
	} // namespace
	
	void printTac(ThreeAddressCode const& tac, sema::SymbolTable const& sym, std::ostream& str) {
		for (auto const& line: tac.statements) {
			std::visit(utl::visitor{
				[&](Label const& label) {
					auto const& function = sym.getFunction(label.functionID);
					str << "  " << function.name() << ".L" << label.index << ":";
				},
				[&](FunctionLabel const& label) {
					auto const& function = sym.getFunction(label.functionID());
					str << function.name() << ":";
				},
				[&](ThreeAddressStatement const& s) {
					str << "    ";
					
					if (!s.result.is(TasArgument::empty)) {
						str << print(s.result, sym) << " = ";
					}
					
					str << s.operation;
					int const argCount = argumentCount(s.operation);
					switch (argCount) {
						case 0:
							break;
						case 1:
							str << " " << print(s.arg1, sym);
							break;
						case 2:
							str << " " << print(s.arg1, sym) << ", " << print(s.arg2, sym);
							break;
						
						SC_NO_DEFAULT_CASE();
					}
				}
			}, line);
			str << "\n";
		}
	}
	
}
