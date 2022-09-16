#include "PrintSymbolTable.h"

#include <iostream>

#include "Basic/PrintUtil.h"

namespace scatha::sem {
	
	void printSymbolTable(SymbolTable const& sym) {
		printSymbolTable(sym, std::cout);
	}

	class internal::ScopePrinter {
	public:
		SymbolTable const& sym;
		void printScope(Scope const* scope, std::ostream& str, int ind);
	};

	static constexpr auto endl = '\n';
	
	static Indenter indent(int level) { return Indenter(level, 2); }
	
	void printSymbolTable(SymbolTable const& sym, std::ostream& str) {
		str << "Global Scope" << endl;
		internal::ScopePrinter p{ sym };
		p.printScope(sym.globalScope(), str, 2);
	}
	
	void internal::ScopePrinter::printScope(Scope const* scope, std::ostream& str, int ind) {
		str << indent(ind - 1) << "Symbols:" << endl;
		for (auto&& [name, id]: scope->_nameToID) {
			str << indent(ind) << name << endl;
		}
		str << indent(ind - 1) << "Child scopes:" << endl;
		for (auto&& [id, child]: scope->_childScopes) {
			printScope(child.get(), str, ind + 2);
		}
	}
	
}
