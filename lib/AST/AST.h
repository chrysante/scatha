#pragma once

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <memory>
#include <iosfwd>
#include <string>

#include <utl/vector.hpp>

namespace scatha::ast {
	
	template <typename T>
	using UniquePtr = std::unique_ptr<T>;
	
	template <typename T, typename... Args> requires std::constructible_from<T, Args...>
	UniquePtr<T> allocate(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
	
	/// MARK: ParseTreeNode
	struct AbstractSyntaxTree {
		struct Indenter;
		virtual ~AbstractSyntaxTree() = default;
		virtual void print(std::ostream&, Indenter&) const = 0;
	};

	std::ostream& operator<<(std::ostream&, AbstractSyntaxTree const&);
	
	struct Expression;
	
	/// MARK: TranslationUnit
	struct TranslationUnit: AbstractSyntaxTree {
		void print(std::ostream&, Indenter&) const override;
		
		utl::small_vector<UniquePtr<AbstractSyntaxTree>> nodes;
	};
	
	/// MARK: Statement
	struct Statement: AbstractSyntaxTree {
		
	};
	
	/// MARK: Declaration
	struct Declaration: Statement {
		explicit Declaration(std::string name): name(std::move(name)) {}
		
		std::string name;
	};
	
	/// MARK: Module
	struct ModuleDeclaration: Declaration {
		
	};
	
	/// MARK: Block
	struct Block: Statement {
		Block() = default;
		explicit Block(utl::vector<UniquePtr<Statement>> statements):
			statements(std::move(statements))
		{}
			
		void print(std::ostream&, Indenter&) const override;
		
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
			Declaration(std::move(name)),
			returnType(std::move(returnType)),
			params(std::move(params))
		{}
		
		void print(std::ostream&, Indenter&) const override;
		
		std::string returnType;
		utl::small_vector<FunctionParameterDecl> params;
	};
	
	/// MARK: FunctionDefiniton
	struct FunctionDefiniton: FunctionDeclaration {
		explicit FunctionDefiniton(FunctionDeclaration const& decl, UniquePtr<Block> body = nullptr):
			FunctionDeclaration(decl),
			body(std::move(body))
		{}
		
		void print(std::ostream&, Indenter&) const override;
			
		UniquePtr<Block> body;
	};
	
	/// MARK: Variable
	struct VariableDeclaration: Declaration {
		using Declaration::Declaration;
		void print(std::ostream&, Indenter&) const override;
		
		bool isConstant = false;
		std::string type;
		UniquePtr<Expression> initExpression;
	};
	
	/// MARK: ExpressionStatement
	struct ExpressionStatement: Statement {
		explicit ExpressionStatement(UniquePtr<Expression>);
		
		~ExpressionStatement();
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: ControlFlow
	struct ControlFlowStatement: Statement {
		
	};
	
	/// MARK: ReturnStatement
	struct ReturnStatement: ControlFlowStatement {
		explicit ReturnStatement(UniquePtr<Expression>);
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> expression;
	};
	
	/// MARK: IfStatement
	struct IfStatement: ControlFlowStatement {
		explicit IfStatement(UniquePtr<Expression> condition,
							 UniquePtr<Block> ifBlock,
							 UniquePtr<Block> elseBlock = nullptr);
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> ifBlock;
		UniquePtr<Block> elseBlock;
	};
	
	/// MARK: WhileStatement
	struct WhileStatement: ControlFlowStatement {
		explicit WhileStatement(UniquePtr<Expression> condition,
									UniquePtr<Block> block);
		
		void print(std::ostream&, Indenter&) const override;
		
		UniquePtr<Expression> condition;
		UniquePtr<Block> block;
	};
	
}

#endif // SCATHA_AST_AST_H_

