#pragma once

#ifndef SCATHA_PARSER_PARSER_H_
#define SCATHA_PARSER_PARSER_H_

#include <memory>
#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Allocator.h"
#include "Common/Token.h"
#include "Common/TypeTable.h"
#include "Common/FunctionTable.h"
#include "Parser/ParseTree.h"
#include "Parser/TokenStream.h"

namespace scatha::parse {
	
	using Allocator = MonotonicBufferAllocator;
	
	class Parser {
	public:
		explicit Parser(std::span<Token const>, Allocator&);
		
		[[nodiscard]] ParseTreeNode* parse();
		
	private:		
		ParseTreeNode* parseRootLevel();
		
		FunctionDeclaration* parseFunction();
		FunctionDeclaration* parseFunctionDecl();
		void parseFunctionParameters(FunctionDeclaration*);
		
		Block* parseBlock();
		
		Statement* parseStatement();
		VariableDeclaration* parseVariableDeclaration(TokenEx const& declarator);
		ReturnStatement* parseReturnStatement();
		
		Expression* parseExpression();
		
		static void expectIdentifier(TokenEx const&);
		static void expectKeyword(TokenEx const&);
		static void expectDeclarator(TokenEx const&);
		static void expectSeparator(TokenEx const&);
		static void expectID(TokenEx const&, std::string_view);
		
	private:
		Allocator& alloc;
		TokenStream tokens;
	};
	
}

#endif // SCATHA_LEXER_LEXER_H_
