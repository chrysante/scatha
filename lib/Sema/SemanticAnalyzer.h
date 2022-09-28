#ifndef SCATHA_SEMA_SEMANTICANALYZER_H_
#define SCATHA_SEMA_SEMANTICANALYZER_H_

#include <stdexcept>

#include "AST/AST.h"
#include "AST/Common.h"
#include "AST/Expression.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {
	
	class SCATHA(API) SemanticAnalyzer {
	public:
		SemanticAnalyzer();
		
		void run(ast::AbstractSyntaxTree*);
		
		SymbolTable const& symbolTable() const { return sym; }
		SymbolTable takeSymbolTable() { return std::move(sym); }
		
	private:
		SymbolTable sym;
	};
	
}

#endif // SCATHA_SEMA_SEMANTICANALYZER_H_

