#include "AST/PrintSource.h"

#include <iostream>

#include <utl/utility.hpp>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Basic/PrintUtil.h"

namespace scatha::ast {

	void printSource(AbstractSyntaxTree const* root) {
		printSource(root, std::cout);
	}
	
	static void printSource_impl(AbstractSyntaxTree const*, std::ostream&, EndlIndenter&);
	static void printSource_impl(AbstractSyntaxTree const*, std::ostream&, EndlIndenter&, NodeType);
	
	void printSource(AbstractSyntaxTree const* root, std::ostream& str) {
		EndlIndenter endl;
		printSource_impl(root, str, endl);
	}
	
	static void printSource_impl(AbstractSyntaxTree const* node, std::ostream& str, EndlIndenter& endl) {
		printSource_impl(node, str, endl, node->nodeType());
	}
	
	static void printSource_impl(AbstractSyntaxTree const* node, std::ostream& str, EndlIndenter& endl, NodeType type) {
		switch (type) {
				using enum NodeType;
			case TranslationUnit: {
				auto const* const tu = static_cast<struct TranslationUnit const*>(node);
				for (auto& n: tu->declarations) {
					printSource_impl(n.get(), str, endl);
					str << endl << endl;
				}
				break;
			}
				
			case Block: {
				auto const* const block = static_cast<struct Block const*>(node);
				str << "{";
				if (block->statements.empty()) {
					str << endl;
				}
				for (auto [i, s]: utl::enumerate(block->statements)) {
					if (i == 0) { endl.increase(); }
					str << endl;
		
					printSource_impl(s.get(), str, endl);
					if (i == block->statements.size() - 1) { endl.decrease(); }
				}
				str << endl << "}";
				break;
			}
			case FunctionDeclaration: {
				auto const* const fn = static_cast<struct FunctionDeclaration const*>(node);
				str << "fn " << fn->name() << "(";
				for (bool first = true; auto const& param: fn->parameters) {
					str << (first ? ((void)(first = false), "") : ", ") << param->name() << ": " << param->declTypename.id;
				}
				str << ") -> " << fn->declReturnTypename.id;
				break;
			}
			case FunctionDefinition: {
				auto const* const fn = static_cast<ast::FunctionDefinition const*>(node);
				printSource_impl(fn, str, endl, NodeType::FunctionDeclaration);
				str << " ";
				printSource_impl(fn->body.get(), str, endl);
				break;
			}
			case StructDeclaration: {
				auto const* const s = static_cast<struct StructDeclaration const*>(node);
				str << "struct " << s->name();
				break;
			}
			case StructDefinition: {
				auto const* const s = static_cast<ast::StructDefinition const*>(node);
				printSource_impl(s, str, endl, NodeType::StructDeclaration);
				str << " ";
				printSource_impl(s->body.get(), str, endl);
				break;
			}
			case VariableDeclaration: {
				auto const* const var = static_cast<struct VariableDeclaration const*>(node);
				str << (var->isConstant ? "let" : "var") << " " << var->name() << ": ";
				str << (var->declTypename.empty() ? "<deduce type>" : var->declTypename.id);
				if (var->initExpression) {
					str << " = ";
					printSource_impl(var->initExpression.get(), str, endl);
				}
				str << ";";
				break;
			}
			case ExpressionStatement: {
				auto const* const es = static_cast<struct ExpressionStatement const*>(node);
				SC_ASSERT(es->expression != nullptr, "");
				printSource_impl(es->expression.get(), str, endl);
				str << ";";
				break;
			}
			case ReturnStatement: {
				auto const* const rs = static_cast<struct ReturnStatement const*>(node);
				str << "return ";
				printSource_impl(rs->expression.get(), str, endl);
				str << ";";
				break;
			}
			case IfStatement: {
				auto const* const is = static_cast<struct IfStatement const*>(node);
				str << "if ";
				printSource_impl(is->condition.get(), str, endl);
				str << " ";
				printSource_impl(is->ifBlock.get(), str, endl);
				if (is->elseBlock) {
					str << endl << "else ";
					printSource_impl(is->elseBlock.get(), str, endl);
				}
				break;
			}
			case WhileStatement: {
				auto const* const ws = static_cast<struct WhileStatement const*>(node);
				str << "while ";
				printSource_impl(ws->condition.get(), str, endl);
				str << " ";
				printSource_impl(ws->block.get(), str, endl);
				break;
			}
				
			case Identifier: {
				auto const* const i = static_cast<struct Identifier const*>(node);
				str << i->value();
				break;
			}
			case IntegerLiteral: {
				auto const* const nl = static_cast<struct IntegerLiteral const*>(node);
				str << nl->value;
				break;
			}
			case BooleanLiteral: {
				auto const* const nl = static_cast<struct BooleanLiteral const*>(node);
				str << (nl->value ? "true" : "false");
				break;
			}
			case FloatingPointLiteral: {
				auto const* const nl = static_cast<struct FloatingPointLiteral const*>(node);
				str << nl->value;
				break;
			}
			case StringLiteral: {
				auto const* const sl = static_cast<struct StringLiteral const*>(node);
				str << '"' << sl->value << '"';
				break;
			}
				
			case UnaryPrefixExpression: {
				auto const* const u = static_cast<struct UnaryPrefixExpression const*>(node);
				str << toString(u->op) << '(';
				printSource_impl(u->operand.get(), str, endl);
				str << ')';
				break;
			}
				
			case BinaryExpression: {
				auto const* const b = static_cast<struct BinaryExpression const*>(node);
				str << '(';
				printSource_impl(b->lhs.get(), str, endl);
				str << ' ' << toString(b->op) << ' ';
				printSource_impl(b->rhs.get(), str, endl);
				str << ')';
				break;
			}
			case MemberAccess: {
				auto const* const ma = static_cast<struct MemberAccess const*>(node);
				printSource_impl(ma->object.get(), str, endl);
				
				str << "." << ma->memberName();
				break;
			}
				
			case Conditional: {
				auto const* const c = static_cast<struct Conditional const*>(node);
				str << "(";
				str << "(";
				printSource_impl(c->condition.get(), str, endl);
				str << ")";
				str << " ? ";
				str << "(";
				printSource_impl(c->ifExpr.get(), str, endl);
				str << ")";
				str << " : ";
				str << "(";
				printSource_impl(c->elseExpr.get(), str, endl);
				str << ")";
				str << ")";
				break;
			}
				
			case FunctionCall: {
				auto const* const f = static_cast<struct FunctionCall const*>(node);
				printSource_impl(f->object.get(), str, endl);
				str << "(";
				for (bool first = true; auto& arg: f->arguments) {
					if (!first) { str << ", "; } else { first = false; }
					printSource_impl(arg.get(), str, endl);
				}
				str << ")";
				break;
			}
			case Subscript: {
				auto const* const f = static_cast<struct Subscript const*>(node);
				printSource_impl(f->object.get(), str, endl);
				str << "(";
				for (bool first = true; auto& arg: f->arguments) {
					if (!first) { str << ", "; } else { first = false; }
					printSource_impl(arg.get(), str, endl);
				}
				str << ")";
				break;
			}
			
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
}
