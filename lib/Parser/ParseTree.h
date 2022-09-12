#ifndef SCATHA_PARSER_PARSETREE_H_
#define SCATHA_PARSER_PARSETREE_H_

#include <string>
#include <span>
#include <iosfwd>

namespace scatha::parse {
	
	/// MARK: ParseTreeNode
	struct ParseTreeNode {
		struct Indenter;
		virtual void print(std::ostream&, Indenter&) const = 0;
	};

	std::ostream& operator<<(std::ostream&, ParseTreeNode const&);
	
	struct Expression;
	
	/// MARK: RootNode
	struct RootNode: ParseTreeNode {
		void print(std::ostream&, Indenter&) const override;
		
		std::span<ParseTreeNode*> nodes;
	};
	
	/// MARK: Statement
	struct Statement: ParseTreeNode {
		
	};
	
	/// MARK: Declaration
	struct Declaration: Statement {
		std::string name;
	};
	
	/// MARK: Module
	struct ModuleDeclaration: Declaration {
		
	};
	
	/// MARK: Block
	struct Block: Statement {
		void print(std::ostream&, Indenter&) const override;
		std::span<Statement*> statements;
	};
	
	/// MARK: Function
	struct FunctionParameterDecl {
		std::string name;
		std::string type;
	};
	
	/// MARK: FunctionDeclaration
	struct FunctionDeclaration: Declaration {
		void print(std::ostream&, Indenter&) const override;
		
		std::string returnType;
		std::span<FunctionParameterDecl> params;
	};
	
	/// MARK: FunctionDefiniton
	struct FunctionDefiniton: FunctionDeclaration {
		FunctionDefiniton() = default;
		explicit FunctionDefiniton(FunctionDeclaration const& decl): FunctionDeclaration(decl) {}
		void print(std::ostream&, Indenter&) const override;
			
		Block* body;
	};
	
	/// MARK: Variable
	struct VariableDeclaration: Declaration {
		void print(std::ostream&, Indenter&) const override;
		
		bool isConstant = false;
		std::string type;
		Expression* initExpression;
	};
	
	/// MARK: ExpressionStatement
	struct ExpressionStatement: Statement {
		Expression* expression;
	};
	
	/// MARK: ControlFlow
	struct ControlFlowStatement: Statement {
		
	};
	
	/// MARK: ReturnStatement
	struct ReturnStatement: ControlFlowStatement {
		void print(std::ostream&, Indenter&) const override;
		
		Expression* expression;
	};
	
	/// MARK: Expression
	struct Expression: ParseTreeNode {
		void print(std::ostream&, Indenter&) const override;
	};
	
}

#endif // SCATHA_PARSER_PARSETREE_H_

