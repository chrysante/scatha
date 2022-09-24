#include "IC/PrintTAC.h"

#include <iostream>

namespace scatha::ic {
	
	void printTAC(TAC const& tac, sema::SymbolTable const& sym) {
		printTAC(tac, sym, std::cout);
	}
	
	namespace {
		struct ElementPrinter {
			ElementPrinter(TASElement const& e, sema::SymbolTable const& sym): e(e), sym(sym) {}
			
			friend std::ostream& operator<<(std::ostream& str, ElementPrinter const& ep) {
				ep.print(str);
				return str;
			}
			
			void print(std::ostream& str) const {
				switch (e.kind) {
					case TASElement::Variable:
						str << "$" << sym.getVariable(sema::SymbolID(e.value, sema::SymbolCategory::Variable)).name();
						break;
					case TASElement::Temporary:
						str << "T[" << e.value << "]";
						break;
					case TASElement::LiteralValue:
						switch (e.type) {
							case TASElement::Bool:
								str << bool(e.value);
								break;
							case TASElement::Unsigned:
								str << e.value;
								break;
							case TASElement::Signed:
								str << static_cast<i64>(e.value);
								break;
							case TASElement::Float:
								str << utl::bit_cast<f64>(e.value);
								break;
						}
						break;
				}
			}
			
			TASElement e;
			sema::SymbolTable const& sym;
		};
		ElementPrinter print(TASElement const& e, sema::SymbolTable const& sym) {
			return ElementPrinter(e, sym);
		}
	}
	
	void printTAC(TAC const& tac, sema::SymbolTable const& sym, std::ostream& str) {
		for (auto const tas: tac.statements) {
			if (tas.isLabel) {
				str << ".L" << tas.label << '\n';
				continue;
			}
			if (tas.op == Operation::jmp) {
				str << "jmp .L" << tas.a << '\n';
				continue;
			}
			if (tas.op == Operation::cjmp) {
				str << "cjmp " << print(tas.getA(), sym) << " .L" << tas.b << '\n';
				continue;
			}
			
			int const argCount = argumentCount(tas.op);
			SC_ASSERT(argCount == 1 || argCount == 2, "");
			str << print(tas.getResult(), sym) << " = ";
			if (argCount == 1) {
				str << toString(tas.op) << " " << print(tas.getA(), sym) << '\n';
				continue;
			}
			str << toString(tas.op) << " " << print(tas.getA(), sym) << ", " << print(tas.getB(), sym) << '\n';
		}
	}
	
}
