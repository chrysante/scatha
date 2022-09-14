#pragma once

#ifndef SCATHA_CODEGENERATOR_CODEGENERATOR_H_
#define SCATHA_CODEGENERATOR_CODEGENERATOR_H_

#include "AST/AST.h"
#include "ExecutionTree/Program.h"

namespace scatha::codegen {
	
	class CodeGenerator {
	public:
		explicit CodeGenerator(ast::AbstractSyntaxTree* ast);
		
		execution::Program generate();
		
	private:
		ast::AbstractSyntaxTree* ast;
	};
	
}

#endif // SCATHA_CODEGENERATOR_CODEGENERATOR_H_
