#ifndef SCATHA_SEMA_CANONICALIZE_H_
#define SCATHA_SEMA_CANONICALIZE_H_

#include "AST/AST.h"

namespace scatha::sema {
	
	
	void canonicalize(ast::AbstractSyntaxTree*);
	
	class Canonicalizer {
	public:
		void run(ast::AbstractSyntaxTree*);
		
		
		
	private:
		void doRun(ast::AbstractSyntaxTree*);
		void doRun(ast::AbstractSyntaxTree*, ast::NodeType);
		
		void doCompoundAssignment(ast::BinaryExpression*,
								  ast::BinaryOperator to) const;
	};
	
}

#endif // SCATHA_SEMA_CANONICALIZE_H_

