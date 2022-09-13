#include "Expression.h"

#include <ostream>

namespace scatha::ast {
	
	void internal::UnaryPrefixExprBase::printImpl(std::ostream& str, AbstractSyntaxTree::Indenter& indent, std::string_view op) const {
		str << op << '(';
		operand->print(str, indent);
		str << ')';
	}
	
	void internal::BinaryExprBase::printImpl(std::ostream& str, AbstractSyntaxTree::Indenter& indent, std::string_view op) const {
		str << '(';
		left->print(str, indent);
		str << ' ' << op << ' ';
		right->print(str, indent);
		str << ')';
	}
	
	void Identifier::print(std::ostream& str, Indenter& indent) const {
		str << name;
	}
	
	void NumericLiteral::print(std::ostream& str, Indenter& indent) const {
		str << value;
	}
	
	void StringLiteral::print(std::ostream& str, Indenter& indent) const {
		str << '"' << value << '"';
	}
	
	void FunctionCall::print(std::ostream& str, Indenter& indent) const {
		object->print(str, indent);
		str << "(";
		for (bool first = true; auto& arg: arguments) {
			if (!first) { str << ", "; } else { first = false; }
			arg->print(str, indent);
		}
		str << ")";
	}

	void Subscript::print(std::ostream& str, Indenter& indent) const {
		object->print(str, indent);
		str << "[";
		for (bool first = true; auto& arg: arguments) {
			if (!first) { str << ", "; } else { first = false; }
			arg->print(str, indent);
		}
		str << "]";
	}
	
	void MemberAccess::print(std::ostream& str, Indenter& indent) const {
		object->print(str, indent);
		str << "." << memberID;
	}
	
	void Conditional::print(std::ostream& str, Indenter& indent) const {
		str << "(";
		str << "(";
		condition->print(str, indent);
		str << ")";
		str << " ? ";
		str << "(";
		if_->print(str, indent);
		str << ")";
		str << " : ";
		str << "(";
		then->print(str, indent);
		str << ")";
		str << ")";
	}
	
	
}
