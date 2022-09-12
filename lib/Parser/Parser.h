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
#include "Parser/TokenEx.h"

namespace scatha::parse {
	
	using Allocator = MonotonicBufferAllocator;
	
	class Parser {
	public:
		explicit Parser(std::span<Token const>, Allocator&);
		
		[[nodiscard]] ParseTreeNode* parse();
		
		std::shared_ptr<TypeTable> getTypeTable() const { return typeTable; }
	
	private:
		enum class State {
			Root = 0, ParsingFunctionDecl, ParsingBlock
		};
		
	private:		
		ParseTreeNode* parseLevelOne();
		FunctionDeclaration* parseFunction();
		FunctionDeclaration* parseFunctionDecl();
		void parseFunctionParameters(FunctionDeclaration*);
		Block* parseBlock();
		Statement* parseStatement();
		VariableDeclaration* parseVariableDeclaration(TokenEx const& declarator);
		ReturnStatement* parseReturnStatement();
		
		
		Expression* parseExpression();
		
		TokenEx const* _getTokenImpl(bool ignoreSeparators, size_t* index);
		TokenEx const* eatToken(bool ignoreSeparators = true);
		TokenEx const* peekToken(bool ignoreSeparators = true);
		
		void prepass();
		
		static void expectIdentifier(TokenEx const&);
		static void expectKeyword(TokenEx const&);
		static void expectDeclarator(TokenEx const&);
		static void expectSeparator(TokenEx const&);
		static void expectID(TokenEx const&, std::string_view);
		
		void ignoreSeparators(size_t* i) const;
		
	private:
		Allocator& alloc;
		std::span<Token const> inTokens;
		utl::vector<TokenEx> exTokens;
		
		std::shared_ptr<TypeTable> typeTable;
		std::shared_ptr<FunctionTable> functionTable;
		
		State state = State::Root;
		
		size_t index = 0;
	};
	
}

#endif // SCATHA_LEXER_LEXER_H_
