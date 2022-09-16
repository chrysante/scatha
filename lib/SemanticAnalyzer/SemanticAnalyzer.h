#ifndef SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_
#define SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_

#include <stdexcept>

#include "AST/AST.h"
#include "AST/Common.h"
#include "AST/Expression.h"
#include "SemanticAnalyzer/SymbolTable.h"

namespace scatha::sem {
	
	class SemanticAnalyzer {
	public:
		SemanticAnalyzer();
		
		void run(ast::AbstractSyntaxTree*);
		
		SymbolTable const& symbolTable() const { return symbols; }
		SymbolTable takeSymbolTable() { return std::move(symbols); }
		
	private:
		void doRun(ast::AbstractSyntaxTree*);
		void doRun(ast::AbstractSyntaxTree*, ast::NodeType);
	
		void verifyConversion(ast::Expression const* from, TypeID to) const;
		
		TypeID verifyBinaryOperation(ast::BinaryExpression const*) const;
		
		void verifyFunctionCallExpression(ast::FunctionCall const*, TypeEx const& fnType, std::span<ast::UniquePtr<ast::Expression> const> arguments) const;
		
		[[noreturn]] void throwBadTypeConversion(Token const&, TypeID from, TypeID to) const;
		
	private:
		bool used = false;
		ast::FunctionDefinition* currentFunction = nullptr;
		SymbolTable symbols;
	};
	
}

#endif // SCATHA_SEMANTICANALYZER_SEMANTICANALYZER_H_

