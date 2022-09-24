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
							case TASElement::Void:
								SC_DEBUGBREAK(); // do we get here?
								break;
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
					SC_NO_DEFAULT_CASE();
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
				auto const& function = sym.getFunction(sema::SymbolID(tas.functionID, sema::SymbolCategory::Function));
				u64 const index = tas.labelIndex;
				str << function.name() << ".L" << index << ":\n";
				continue;
			}
			
			str << "    ";
			
			if (isJump(tas.op)) {
				str << toString(tas.op) << " ";
				if (tas.op == Operation::cjmp) {
					str << print(tas.getB(), sym) << ", ";
				}
				auto const& function = sym.getFunction(sema::SymbolID(tas.functionID,
																	  sema::SymbolCategory::Function));
				str << function.name() << ".L" << tas.labelIndex << '\n';
				continue;
			}
			
			if (auto const resultElem = tas.getResult(); resultElem.type != TASElement::Void) {
				str << print(resultElem, sym) << " = ";
			}
			
			int const argCount = argumentCount(tas.op);
			switch (argCount) {
				case 0:
					str << toString(tas.op) << '\n';
					break;
				case 1:
					SC_ASSERT(tas.aKind != TASElement::Label, "");
					str << toString(tas.op) << " " << print(tas.getA(), sym) << '\n';
					break;
				case 2:
					str << toString(tas.op) << " " << print(tas.getA(), sym) << ", " << print(tas.getB(), sym) << '\n';
					break;
				
				SC_NO_DEFAULT_CASE();
			}
		}
	}
	
}
