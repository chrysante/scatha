#include "AST/AST.h"

#include <ostream>

#include <utl/utility.hpp>

#include "AST/Expression.h"

namespace scatha::ast {

	struct AbstractSyntaxTree::Indenter {
		Indenter& increase()& { ++level; return *this; }
		Indenter& decrease()& { --level; return *this; }
		int level = 0;
	};
	
	static std::ostream& operator<<(std::ostream& str, AbstractSyntaxTree::Indenter const& indent) {
		str << '\n';
		for (int i = 0; i < indent.level; ++i) {
			str << '\t';
		}
		return str;
	}
	
	/// MARK: AbstractSyntaxTree
	std::ostream& operator<<(std::ostream& str, AbstractSyntaxTree const& node) {
		AbstractSyntaxTree::Indenter i;
		node.print(str, i);
		return str;
	}

	/// MARK: TranslationUnit
	void TranslationUnit::print(std::ostream& str, Indenter& indent) const {
		for (auto& n: nodes) {
			n->print(str, indent);
			str << indent << indent;
		}
	}
	
	/// MARK: Statement
	
	/// MARK: Declaration

	/// MARK: Module
	
	/// MARK: Block
	void Block::print(std::ostream& str, Indenter& indent) const {
		str << "{";
		if (statements.empty()) {
			str << indent;
		}
		for (auto [i, s]: utl::enumerate(statements)) {
			if (i == 0) { indent.increase(); }
			str << indent;
			
			s->print(str, indent);
			if (i == statements.size() - 1) { indent.decrease(); }
		}
		str << indent << "}";
	}
	
	/// MARK: Function
	
	/// MARK: FunctionDeclaration
	void FunctionDeclaration::print(std::ostream& str, Indenter& indent) const {
		str << "fn " << name << "(";
		for (bool first = true; auto const& param: params) {
			str << (first ? ((void)(first = false), "") : ", ") << param.name << ": " << param.type;
		}
		str << ") -> " << returnType;
	}
	
	/// MARK: FunctionDefiniton
	void FunctionDefiniton::print(std::ostream& str, Indenter& indent) const {
		FunctionDeclaration::print(str, indent);
		str << " ";
		body->print(str, indent);
	}
	
	/// MARK: VariableDeclaration
	void VariableDeclaration::print(std::ostream& str, Indenter& indent) const {
		str << (isConstant ? "let" : "var") << " " << name << ": ";
		str << (type.empty() ? "<deduce type>" : type);
		if (initExpression) {
			str << " = " << *initExpression;
		}
	}
	
	/// MARK: ExpressionStatement
	ExpressionStatement::ExpressionStatement(UniquePtr<Expression> expression):
		expression(std::move(expression))
	{}
	ExpressionStatement::~ExpressionStatement() = default;
	
	void ExpressionStatement::print(std::ostream& str, Indenter& indent) const {
		if (!expression) {
			return;
		}
		expression->print(str, indent);
	}

	/// MARK: ReturnStatement
	ReturnStatement::ReturnStatement(UniquePtr<Expression> expression):
		expression(std::move(expression))
	{}
	
	void ReturnStatement::print(std::ostream& str, Indenter& indent) const {
		str << "return " << *expression;
	}
	
	/// MARK: IfStatement
	IfStatement::IfStatement(UniquePtr<Expression> condition, UniquePtr<Block> ifBlock, UniquePtr<Block> elseBlock):
		condition(std::move(condition)),
		ifBlock(std::move(ifBlock)),
		elseBlock(std::move(elseBlock))
	{}
	
	void IfStatement::print(std::ostream& str, Indenter& indent) const {
		str << "if ";
		condition->print(str, indent);
		str << " ";
		ifBlock->print(str, indent);
		
		if (elseBlock) {
			str << indent << "else ";
			elseBlock->print(str, indent);
		}
	}
	
	WhileStatement::WhileStatement(UniquePtr<Expression> condition, UniquePtr<Block> block):
		condition(std::move(condition)),
		block(std::move(block))
	{}
	
	void WhileStatement::print(std::ostream& str, Indenter& indent) const {
		str << "while ";
		condition->print(str, indent);
		str << " ";
		block->print(str, indent);
	}
	
}
