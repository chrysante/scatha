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
		TASElement doRun(ast::AbstractSyntaxTree const*);
		
		TASElement submit(TASElement result, Operation, TASElement a, TASElement b = {});
		TASElement submit(Operation, TASElement a, TASElement b = {});
		[[nodiscard]] size_t submitJump();
		[[nodiscard]] size_t submitCJump(TASElement cond);
		size_t submitLabel();
		
		Operation selectOperation(sema::TypeID, ast::BinaryOperator) const;
		TASElement::Type mapFundType(sema::TypeID) const;
		
	private:
		sema::SymbolTable const& sym;
		
		utl::vector<TAS> code;
		size_t tmpIndex = 0;
		size_t labelIndex = 0;
	};
	
}

#endif // SCATHA_IC_TACGENERATOR_H_

