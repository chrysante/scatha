#ifndef SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_
#define SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_

#include <stdexcept>

#include "AST/AST.h"
#include "AST/Common.h"
#include "Common/IdentifierTable.h"

namespace scatha::sem {
	
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
	
	class SemanticAnalyzer {
	public:
		SemanticAnalyzer();
		
		void run(ast::AbstractSyntaxTree*);
		
	private:
		void doRun(ast::AbstractSyntaxTree*);
		void doRun(ast::AbstractSyntaxTree*, ast::NodeType);
	
		void verifyConversion(ast::Expression const* from, TypeID to);
		
		
		TypeID verifyBinaryOperation(ast::BinaryExpression const*);
		
		bool used = false;
		ast::FunctionDefinition* currentFunction = nullptr;
		IdentifierTable identifiers;
	};
	
}

#endif // SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_

