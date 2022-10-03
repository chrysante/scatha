#ifndef SCATHA_SEMA_ANALYZE_H_
#define SCATHA_SEMA_ANALYZE_H_

#include <stdexcept>

#include "AST/Base.h"
#include "Issue/IssueHandler.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

	/**
	 Function \p sema::analzye()
	 Semantically analyzes and decorates the abstract syntax tree.
	 
	 - parameter root: Root of the tree to analyze.
	 - returns: The generated symbol table
	 
	 -
	 
	 # Notes: #
	 */
	SCATHA(API) SymbolTable analyze(ast::AbstractSyntaxTree* root, issue::IssueHandler&);
	
}

#endif // SCATHA_SEMA_ANALYZE_H_

