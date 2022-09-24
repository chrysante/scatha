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
		[[nodiscard]] TAC run(ast::AbstractSyntaxTree const*);
		
	private:
		TASElement doRun(ast::AbstractSyntaxTree const*);
		
		TASElement submitVarAssignUnary(TASElement result, Operation, TASElement a);
		TASElement submitVarAssignBinary(TASElement result, Operation, TASElement a, TASElement b);
		TASElement submitTempNullary(Operation, TASElement::Type type);
		TASElement submitTempUnary(Operation, TASElement a);
		TASElement submitTempBinary(Operation, TASElement a, TASElement b);
		void submitVoid(Operation, TASElement a);
		[[nodiscard]] size_t submitJump();
		[[nodiscard]] size_t submitCJump(TASElement cond);
		void submitCall(sema::SymbolID functionID);
		size_t submitLabel();
		
		Operation selectOperation(sema::TypeID, ast::BinaryOperator) const;
		TASElement::Type mapFundType(sema::TypeID) const;
		
	private:
		sema::SymbolTable const& sym;
		
		
		utl::vector<TAS> code;
		size_t tmpIndex = 0;
		
		// Will be set by the FunctionDefinition case
		sema::SymbolID currentFunctionID;
		// Will be reset to 0 by the FunctionDefinition case
		size_t labelIndex = 0;
	};
	
}

#endif // SCATHA_IC_TACGENERATOR_H_

