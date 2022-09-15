#ifndef SCATHA_AST_TYPECHECKER_H_
#define SCATHA_AST_TYPECHECKER_H_

#include <stdexcept>

#include "AST/AST.h"
#include "AST/Operator.h"
#include "Common/IdentifierTable.h"

namespace scatha::ast {
	
	struct TypeError: std::runtime_error {
		explicit TypeError(std::string_view brief, Token const& token, std::string_view message = {}):
			std::runtime_error(makeString(brief, token, message))
		
		{}

	private:
		static std::string makeString(std::string_view brief, Token const& token, std::string_view message);
	};
	
	struct ImplicitConversionError: TypeError {
		ImplicitConversionError(IdentifierTable const&, TypeID from, TypeID to, Token const& token);
	};
	
	class TypeChecker {
	public:
		TypeChecker();
		
		void run(AbstractSyntaxTree*);
		
	private:
		void doRun(AbstractSyntaxTree*);
		void doRun(AbstractSyntaxTree*, NodeType);
	
		void verifyConversion(Expression const* from, TypeID to);
		
		
		TypeID verifyBinaryOperation(BinaryExpression const*);
		
		bool used = false;
		FunctionDefinition* currentFunction = nullptr;
		IdentifierTable identifiers;
	};
	
}

#endif // SCATHA_AST_TYPECHECKER_H_

