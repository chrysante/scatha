#ifndef SCATHA_IC_TACGENERATOR_H_
#define SCATHA_IC_TACGENERATOR_H_

#include "AST/AST.h"
#include "AST/Common.h"
#include "IC/ThreeAddressCode.h"
#include "IC/ThreeAddressStatement.h"
#include "Sema/SemanticElements.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	class TacGenerator {
	public:
		explicit TacGenerator(sema::SymbolTable const&);
		[[nodiscard]] ThreeAddressCode run(ast::AbstractSyntaxTree const*);
		
	private:
		void doRun(ast::Statement const*);
		TasArgument doRun(ast::Expression const*);
		
		void submit(Operation, TasArgument a = {}, TasArgument b = {});
		TasArgument submit(TasArgument result, Operation,
						   TasArgument a = {}, TasArgument b = {});
				
		// Returns the code position of the submitted jump,
		// so the label can be updated later
		size_t submitJump(Operation, TasArgument cond = {}, Label label = {});
		
		// Returns the label
		Label submitLabel();
		
		FunctionLabel submitFunctionLabel(ast::FunctionDefinition const&);
		
		TasArgument makeTemporary(sema::TypeID type);

		Operation selectOperation(sema::TypeID, ast::BinaryOperator) const;
		
	private:
		sema::SymbolTable const& sym;
		utl::vector<TacLine> code;
		
		size_t tmpIndex = 0;
		
		// Will be set by the FunctionDefinition case
		sema::SymbolID currentFunctionID;
		// Will be reset to 0 by the FunctionDefinition case
		size_t labelIndex = 0;
	};
	
}

#endif // SCATHA_IC_TACGENERATOR_H_

