#include "Sema/PrintSymbolTable.h"

#include <iostream>

#include <utl/hashset.hpp>

#include "Basic/PrintUtil.h"

namespace scatha::sema {
	
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
		internal::ScopePrinter p{ sym };
		p.printScope(sym.globalScope(), str, 0);
	}
	
	void internal::ScopePrinter::printScope(Scope const* scope, std::ostream& str, int ind) {
		utl::hashset<SymbolID> printedScopes;
		if (!scope->_nameIDMap.empty()) {
			for (auto&& [name, id]: scope->_nameIDMap) {
				
				str << indent(ind) << toString(id.category()) << " " << name << endl;
				auto const itr = scope->_childScopes.find(id);
				if (itr == scope->_childScopes.end()) {
					continue;
				}
				auto const [_, insertSuccess] = printedScopes.insert(id);
				SC_ASSERT(insertSuccess, "");
				auto const& childScope = itr->second;
				printScope(childScope.get(), str, ind+1);
			}
		}
		
		for (auto&& [id, childScope]: scope->_childScopes) {
			if (printedScopes.contains(id)) {
				continue;
			}
			str << indent(ind) << "<anonymous-scope>" << endl;
			printScope(childScope.get(), str, ind+1);
		}
	}
	
}
