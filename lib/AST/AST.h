#pragma once

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <memory>
#include <iosfwd>
#include <string>

#include <utl/vector.hpp>

#include "AST/ASTBase.h"
#include "AST/Expression.h"
#include "AST/NodeType.h"
#include "Common/Type.h"

namespace scatha::ast {
	
	/// MARK: TranslationUnit
	struct TranslationUnit: AbstractSyntaxTree {
		TranslationUnit(): AbstractSyntaxTree(NodeType::TranslationUnit, Token{}) {}
		
		utl::small_vector<UniquePtr<AbstractSyntaxTree>> nodes;
	};
	
	/// MARK: Statement
	struct Statement: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
	};
	
	/// MARK: Declaration
	struct Declaration: Statement {
	protected:
		explicit Declaration(NodeType type, Token const& token):
			Statement(type, token),
			name(allocate<Identifier>(token))
		{}
		
	public:
		UniquePtr<Identifier> name;
	};
	
	/// MARK: Variable
	struct VariableDeclaration: Declaration {
		explicit VariableDeclaration(Token const& name):
			Declaration(NodeType::VariableDeclaration, name)
		{}
		
		bool isConstant = false;
		
		// Declared typename in the source code
		std::string declTypename;
		
		TypeID typeID{};
		UniquePtr<Expression> initExpression;
	};
	
	/// MARK: Module
	struct ModuleDeclaration: Declaration {
		ModuleDeclaration() = delete;
	};
	
	/// MARK: Block
	struct Block: Statement {
		explicit Block(Token const& token): Statement(NodeType::Block, token) {}
		
		utl::small_vector<UniquePtr<Statement>> statements;
	};
	
	/// MARK: FunctionDeclaration
	struct FunctionDeclaration: Declaration {
		explicit FunctionDeclaration(Token const& name,
									 Token const& declReturnTypename = {},
									 utl::vector<UniquePtr<VariableDeclaration>> params = {}):
			Declaration(NodeType::FunctionDeclaration, std::move(name)),
			declReturnTypename(std::move(declReturnTypename)),
			params(std::move(params))
		{}
		
		// Declared typename in the source code
		Token declReturnTypename;
		TypeID returnTypeID{};
		utl::small_vector<UniquePtr<VariableDeclaration>> params;
	};
	
	/// MARK: FunctionDefinition
	struct FunctionDefinition: FunctionDeclaration {
		explicit FunctionDefinition(FunctionDeclaration&& decl, UniquePtr<Block> body = nullptr):
			FunctionDeclaration(std::move(decl)),
			body(std::move(body))
		{ _type = NodeType::FunctionDefinition; }
			
		UniquePtr<Block> body;
	};
	
	/// MARK: ExpressionStatement
	struct ExpressionStatement: Statement {
		explicit ExpressionStatement(UniquePtr<Expression> expression, Token const& token):
			Statement(NodeType::ExpressionStatement, token),
			expression(std::move(expression))
		{}
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: ControlFlow
	struct ControlFlowStatement: Statement {
	protected:
		using Statement::Statement;
	};
	
	/// MARK: ReturnStatement
	struct ReturnStatement: ControlFlowStatement {
		explicit ReturnStatement(UniquePtr<Expression> expression, Token const& token):
			ControlFlowStatement(NodeType::ReturnStatement, token),
			expression(std::move(expression))
		{}
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: IfStatement
	struct IfStatement: ControlFlowStatement {
		explicit IfStatement(UniquePtr<Expression> condition, Token const& token):
			ControlFlowStatement(NodeType::IfStatement, token),
			condition(std::move(condition))
		{}
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> ifBlock;
		UniquePtr<Block> elseBlock;
	};
	
	/// MARK: WhileStatement
	struct WhileStatement: ControlFlowStatement {
		explicit WhileStatement(UniquePtr<Expression> condition, Token const& token):
			ControlFlowStatement(NodeType::WhileStatement, token),
			condition(std::move(condition))
		{}
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> block;
	};
	
}

#endif // SCATHA_AST_AST_H_

