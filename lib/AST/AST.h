#pragma once

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <memory>
#include <iosfwd>
#include <string>

#include <utl/vector.hpp>

#include "AST/NodeType.h"

namespace scatha::ast {
	
	template <typename T>
	using UniquePtr = std::unique_ptr<T>;
	
	template <typename T, typename... Args> requires std::constructible_from<T, Args...>
	UniquePtr<T> allocate(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	
	/// MARK: ParseTreeNode
	struct AbstractSyntaxTree {
	public:
		virtual ~AbstractSyntaxTree() = default;
		NodeType nodeType() const { return _type; }
	
	protected:
		explicit AbstractSyntaxTree(NodeType type):
			_type(type)
		{}
		
	private:
		friend class FunctionDefinition;
		NodeType _type;
	};

	std::ostream& operator<<(std::ostream&, AbstractSyntaxTree const&);
	
	struct Expression;
	
	/// MARK: TranslationUnit
	struct TranslationUnit final: AbstractSyntaxTree {
		TranslationUnit(): AbstractSyntaxTree(NodeType::TranslationUnit) {}
		
		utl::small_vector<UniquePtr<AbstractSyntaxTree>> nodes;
	};
	
	/// MARK: Statement
	struct Statement: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
	};
	
	/// MARK: Declaration
	struct Declaration: Statement {
	protected:
		explicit Declaration(NodeType type, std::string name):
			Statement(type),
			name(std::move(name))
		{}
		
	public:
		std::string name;
	};
	
	/// MARK: Module
	struct ModuleDeclaration: Declaration {
		
	};
	
	/// MARK: Block
	struct Block: Statement {
		Block(): Statement(NodeType::Block) {}
		explicit Block(utl::vector<UniquePtr<Statement>> statements):
			Statement(NodeType::Block),
			statements(std::move(statements))
		{}
		
		utl::small_vector<UniquePtr<Statement>> statements;
	};
	
	/// MARK: Function
	struct FunctionParameterDecl {
		std::string name;
		std::string type;
	};
	
	/// MARK: FunctionDeclaration
	struct FunctionDeclaration: Declaration {
		explicit FunctionDeclaration(std::string name,
									 std::string returnType = {},
									 utl::vector<FunctionParameterDecl> params = {}):
			Declaration(NodeType::FunctionDeclaration, std::move(name)),
			returnType(std::move(returnType)),
			params(std::move(params))
		{}
		
		std::string returnType;
		utl::small_vector<FunctionParameterDecl> params;
	};
	
	/// MARK: FunctionDefinition
	struct FunctionDefinition: FunctionDeclaration {
		explicit FunctionDefinition(FunctionDeclaration const& decl, UniquePtr<Block> body = nullptr):
			FunctionDeclaration(decl),
			body(std::move(body))
		{ _type = NodeType::FunctionDefinition; }
			
		UniquePtr<Block> body;
	};
	
	/// MARK: Variable
	struct VariableDeclaration: Declaration {
		explicit VariableDeclaration(std::string name);
		
		bool isConstant = false;
		std::string type;
		UniquePtr<Expression> initExpression;
	};
	
	/// MARK: ExpressionStatement
	struct ExpressionStatement: Statement {
		explicit ExpressionStatement(UniquePtr<Expression>);
		
		~ExpressionStatement();
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: ControlFlow
	struct ControlFlowStatement: Statement {
	protected:
		using Statement::Statement;
	};
	
	/// MARK: ReturnStatement
	struct ReturnStatement: ControlFlowStatement {
		explicit ReturnStatement(UniquePtr<Expression>);
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: IfStatement
	struct IfStatement: ControlFlowStatement {
		explicit IfStatement(UniquePtr<Expression> condition,
							 UniquePtr<Block> ifBlock,
							 UniquePtr<Block> elseBlock = nullptr);
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> ifBlock;
		UniquePtr<Block> elseBlock;
	};
	
	/// MARK: WhileStatement
	struct WhileStatement: ControlFlowStatement {
		explicit WhileStatement(UniquePtr<Expression> condition,
									UniquePtr<Block> block);
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> block;
	};
	
}

#endif // SCATHA_AST_AST_H_

