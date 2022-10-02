#pragma once

#ifndef SCATHA_AST_AST_H_
#define SCATHA_AST_AST_H_

#include <concepts>
#include <memory>
#include <iosfwd>
#include <string>

#include <utl/vector.hpp>

#include "AST/Base.h"
#include "AST/Common.h"
#include "AST/Expression.h"
#include "Sema/SymbolID.h"
#include "Sema/ScopeKind.h"
#include "Sema/Scope.h"

/*
 AbstractSyntaxTree
 +- TranslationUnit
 +- Statement
 |  +- Declaration
 |  |  +- VariableDeclaration
 |  |  +- ModuleDeclaration
 |  |  +- FunctionDefinition
 |  |  +- StructDefinition
 |  +- Block
 |  +- ExpressionStatement
 |  +- ControlFlowStatement
 |     +- ReturnStatement
 |     +- IfStatement
 |     +- WhileStatement
 +- Expression
    +- Identifier
    +- IntegerLiteral
    +- StringLiteral
    +- UnaryPrefixExpression
    +- BinaryExpression
    +- MemberAccess
    +- Conditional
    +- FunctionCall
    +- Subscript
 */

namespace scatha::ast {
	
	/// MARK: Statement
	/// Abstract node representing any statement.
	struct SCATHA(API) Statement: AbstractSyntaxTree {
		using AbstractSyntaxTree::AbstractSyntaxTree;
	};
	
	/// MARK: Declaration
	/// Abstract node representing any declaration.
	struct SCATHA(API) Declaration: Statement {
	public:
		/// Name of the declared symbol as written in the source code.
		std::string_view name() const { return _token.id; }
		
		/// Token object of the name in the source code.
		Token const& token() const { return _token; }

		/** Decoration provided by semantic analysis. */
		
		/// SymbolID of this declaration in the symbol table
		sema::SymbolID symbolID;
		
	protected:
		explicit Declaration(NodeType type, Token const& token):
			Statement(type, token),
			_token(token)
		{}
		
	private:
		Token _token;
	};
	
	/// MARK: TranslationUnit
	/// Concrete node representing a translation unit.
	struct SCATHA(API) TranslationUnit: AbstractSyntaxTree {
		TranslationUnit(): AbstractSyntaxTree(NodeType::TranslationUnit, Token{}) {}
		
		/// List of declarations in the translation unit.
		utl::small_vector<UniquePtr<Declaration>> declarations;
	};
	
	/// MARK: VariableDeclaration
	/// Concrete node representing a variable declaration.
	struct SCATHA(API) VariableDeclaration: Declaration {
		explicit VariableDeclaration(Token const& name):
			Declaration(NodeType::VariableDeclaration, name)
		{}
		
		bool isConstant              : 1 = false; // Will be set by the parser
		bool isFunctionParameter     : 1 = false; // Will be set by the parser
		bool isFunctionParameterDef  : 1 = false; // Will be set by the constructor of FunctionDefinition during parsing
		
		/// Typename declared in the source code. Null if no typename was declared.
		UniquePtr<Expression> typeExpr;
		
		/// Expression to initialize this variable.
		UniquePtr<Expression> initExpression;
		
		
		/** Decoration provided by semantic analysis. */
		
		/// Type of the variable.
		/// Either deduced by the type of \p initExpression or by \p declTypename and then checked against the type of \p initExpression
		sema::TypeID typeID = sema::TypeID::Invalid;
	};
	
	/// MARK: Module
	/// Nothing to see here yet...
	struct SCATHA(API) ModuleDeclaration: Declaration {
		ModuleDeclaration() = delete;
	};
	
	/// MARK: Block
	/// Concrete node representing any block like a function or loop body. Declares its own (maybe anonymous) scope.
	struct SCATHA(API) Block: Statement {
		explicit Block(Token const& token): Statement(NodeType::Block, token) {}
		
		/// Statements in the block.
		utl::small_vector<UniquePtr<Statement>> statements;
		
		/** Decoration provided by semantic analysis. */
		
		/// Kind of this block scope
		sema::ScopeKind scopeKind = sema::ScopeKind::Anonymous;
		
		/// SymbolID of this block scope
		sema::SymbolID scopeSymbolID{};
	};
	
	/// MARK: FunctionDefinition
	/// Concrete node representing the definition of a function.
	struct SCATHA(API) FunctionDefinition: Declaration {
		explicit FunctionDefinition(Token const& name):
			Declaration(NodeType::FunctionDefinition, std::move(name))
		{}
		
		/// Typename of the return type as declared in the source code.
		/// Will be implicitly Identifier("void") if no return type was declared.
		UniquePtr<Expression> returnTypeExpr;
		
		/// List of parameter declarations.
		utl::small_vector<UniquePtr<VariableDeclaration>> parameters;
		
		/// Body of the function.
		UniquePtr<Block> body;
		
		/** Decoration provided by semantic analysis. */
		
		/// Return type of the function.
		sema::TypeID returnTypeID = sema::TypeID::Invalid;
		
		/// Type of the function.
		sema::TypeID functionTypeID = sema::TypeID::Invalid;
	};
	
	/// MARK: StructDefinition
	/// Concrete node representing the definition of a struct.
	struct SCATHA(API) StructDefinition: Declaration {
		explicit StructDefinition(Token const& name):
			Declaration(NodeType::StructDefinition, std::move(name))
		{}
		
		/// Body of the struct.
		UniquePtr<Block> body;
		
		/** Decoration provided by semantic analysis. */
		
		/// Type of this type.
		sema::TypeID typeID = sema::TypeID::Invalid;
	};
	
	/// MARK: ExpressionStatement
	/// Concrete node representing a statement that consists of a single expression.
	/// May only appear at function scope.
	struct SCATHA(API) ExpressionStatement: Statement {
		explicit ExpressionStatement(UniquePtr<Expression> expression, Token const& token):
			Statement(NodeType::ExpressionStatement, token),
			expression(std::move(expression))
		{}
		
		/// The expression
		UniquePtr<Expression> expression;
	};
	
	/// MARK: ControlFlow
	/// Abstract node representing any control flow statement like if, while, for, return etc.
	/// May only appear at function scope.
	struct SCATHA(API) ControlFlowStatement: Statement {
	protected:
		using Statement::Statement;
	};
	
	/// MARK: ReturnStatement
	/// Concrete node representing a return statement.
	struct SCATHA(API) ReturnStatement: ControlFlowStatement {
		explicit ReturnStatement(UniquePtr<Expression> expression, Token const& token):
			ControlFlowStatement(NodeType::ReturnStatement, token),
			expression(std::move(expression))
		{}
		
		/// The returned expression. May be null in case of a void function.
		UniquePtr<Expression> expression;
	};
	
	/// MARK: IfStatement
	/// Concrete node representing an if/else statement.
	struct SCATHA(API) IfStatement: ControlFlowStatement {
		explicit IfStatement(UniquePtr<Expression> condition, Token const& token):
			ControlFlowStatement(NodeType::IfStatement, token),
			condition(std::move(condition))
		{}
		
		/// Condition to branch on.
		/// Must not be null after parsing and must be of type bool (or maybe later convertible to bool).
		UniquePtr<Expression> condition;
		UniquePtr<Statement> ifBlock;
		UniquePtr<Statement> elseBlock;
	};
	
	/// MARK: WhileStatement
	/// Concrete node representing a while statement.
	struct SCATHA(API) WhileStatement: ControlFlowStatement {
		explicit WhileStatement(UniquePtr<Expression> condition, Token const& token):
			ControlFlowStatement(NodeType::WhileStatement, token),
			condition(std::move(condition))
		{}
		
		/// Condition to loop on.
		/// Must not be null after parsing and must be of type bool (or maybe later convertible to bool).
		UniquePtr<Expression> condition;
		UniquePtr<Block> block;
	};
	
}

#endif // SCATHA_AST_AST_H_

