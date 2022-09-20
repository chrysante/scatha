#ifndef SCATHA_IC_TACGENERATOR_H_
#define SCATHA_IC_TACGENERATOR_H_

#include "AST/AST.h"
#include "AST/Common.h"
#include "IC/TAS.h"
#include "Sema/SemanticElements.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	class TACGenerator {
	public:
		explicit TACGenerator(sema::SymbolTable const&);
		[[nodiscard]] TAC run(ast::FunctionDefinition const*);
		
	private:
		TAS::Element doRun(ast::AbstractSyntaxTree const*);
		
		TAS::Element submit(TAS::Element result, Operation, TAS::Element a, TAS::Element b = {});
		TAS::Element submit(Operation, TAS::Element a, TAS::Element b = {});
		[[nodiscard]] size_t submitJump();
		[[nodiscard]] size_t submitCJump(TAS::Element cond);
		size_t submitLabel();
		
		Operation selectOperation(sema::TypeID, ast::BinaryOperator) const;
		
	private:
		sema::SymbolTable const& sym;
		
		utl::vector<TAS> code;
		size_t tmpIndex = 0;
		size_t labelIndex = 0;
	};
	
}

#endif // SCATHA_IC_TACGENERATOR_H_

