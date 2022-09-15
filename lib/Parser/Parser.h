#pragma once

#ifndef SCATHA_PARSER_PARSER_H_
#define SCATHA_PARSER_PARSER_H_

#include <memory>
#include <span>

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Parser/TokenStream.h"

namespace scatha::parse {
	
	class Parser {
	public:
		explicit Parser(std::span<Token const>);
		
		[[nodiscard]] ast::UniquePtr<ast::AbstractSyntaxTree> parse();
		
	private:		
		ast::UniquePtr<ast::Declaration> parseRootLevelDeclaration();
		
		ast::UniquePtr<ast::FunctionDeclaration> parseFunction();
		ast::UniquePtr<ast::FunctionDeclaration> parseFunctionDecl();
		void parseFunctionParameters(ast::FunctionDeclaration*);
		
		ast::UniquePtr<ast::Block> parseBlock();
		
		ast::UniquePtr<ast::Statement> parseStatement();
		ast::UniquePtr<ast::VariableDeclaration> parseVariableDeclaration(bool isFunctionParameter = false);
		ast::UniquePtr<ast::ReturnStatement> parseReturnStatement();
		ast::UniquePtr<ast::IfStatement> parseIfStatement();
		ast::UniquePtr<ast::WhileStatement> parseWhileStatement();
		
		ast::UniquePtr<ast::Expression> parseExpression();
		
	private:
		TokenStream tokens;
	};
	
}

#endif // SCATHA_LEXER_LEXER_H_
