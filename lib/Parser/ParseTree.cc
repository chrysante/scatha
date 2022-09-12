#include "Parser/ParseTree.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::parse {
	
	struct ParseTreeNode::Indenter {
		Indenter& increase()& { ++level; return *this; }
		Indenter& decrease()& { --level; return *this; }
		int level = 0;
	};
	
	static std::ostream& operator<<(std::ostream& str, ParseTreeNode::Indenter const& indent) {
		str << '\n';
		for (int i = 0; i < indent.level; ++i) {
			str << '\t';
		}
		return str;
	}
	
	/// MARK: ParseTreeNode
	std::ostream& operator<<(std::ostream& str, ParseTreeNode const& node) {
		ParseTreeNode::Indenter i;
		node.print(str, i);
		return str;
	}

	/// MARK: RootNode
	void RootNode::print(std::ostream& str, Indenter& indent) const {
		for (auto n: nodes) {
			n->print(str, indent);
		}
	}
	
	/// MARK: Statement
	
	/// MARK: Declaration

	/// MARK: Module
	
	/// MARK: Block
	void Block::print(std::ostream& str, Indenter& indent) const {
		str << "{" << indent.increase();
		for (auto [i, s]: utl::enumerate(statements)) {
			s->print(str, indent);
			if (i == statements.size() - 1) { indent.decrease(); }
			str << indent;
		}
		str << "}" << indent;
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
		str << " = " << *initExpression;
	}

	/// MARK: ReturnStatement
	void ReturnStatement::print(std::ostream& str, Indenter& indent) const {
		str << "return " << *expression;
	}
	
	/// MARK: Expression
	void Expression::print(std::ostream& str, Indenter& indent) const {
		str << "<empty expression>";
	}
	
}
