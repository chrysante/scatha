#include "PrintTree.h"

#include <iostream>

#include "AST/Expression.h"
#include "Basic/Basic.h"
#include "Basic/PrintUtil.h"

namespace scatha::ast {

	static constexpr char endl = '\n';
	
	static Indenter indent(int level) {
		return Indenter(level, 2);
	}
		
	void printTree(AbstractSyntaxTree const* root) {
		printTree(root, std::cout);
	}
	
	static void printTreeImpl(AbstractSyntaxTree const* node, std::ostream& str, int indent);
	static void printTreeImpl(AbstractSyntaxTree const* node, std::ostream& str, int indent, NodeType type);
	
	void printTree(AbstractSyntaxTree const* root, std::ostream& str) {
		printTreeImpl(root, str, 0);
	}
	
	static void printTreeImpl(AbstractSyntaxTree const* node, std::ostream& str, int indent) {
		printTreeImpl(node, str, indent, node->nodeType());
	}
	
	static void printTreeImpl(AbstractSyntaxTree const* inNode, std::ostream& str, int ind, NodeType type) {
		switch (type) {
			case NodeType::TranslationUnit: {
				auto const* const tu = static_cast<TranslationUnit const*>(inNode);
				str << indent(ind) << "<translation-unit>" << endl;
				for (auto& decl: tu->declarations) {
					printTreeImpl(decl.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::Block: {
				auto const* const node = static_cast<Block const*>(inNode);
				str << indent(ind) << "<block>" << endl;
				for (auto& n: node->statements) {
					printTreeImpl(n.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::FunctionDeclaration: {
				auto const* const node = static_cast<FunctionDeclaration const*>(inNode);
				str << indent(ind) << "<function-declaration> : " << node->name() << " -> " << node->declReturnTypename.id << endl;
				for (auto& p: node->parameters) {
					printTreeImpl(p.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::FunctionDefinition: {
				auto const* const node = static_cast<FunctionDefinition const*>(inNode);
				str << indent(ind) << "<function-definition>: " << node->name() << " -> " << node->declReturnTypename.id << endl;
				printTreeImpl(node, str, ind + 1, NodeType::FunctionDeclaration);
				printTreeImpl(node->body.get(), str, ind + 1);
				break;
			}
				
			case NodeType::StructDeclaration: {
				auto const* const node = static_cast<StructDeclaration const*>(inNode);
				str << indent(ind) << "<struct-declaration> : " << node->name() << endl;
				break;
			}
			
			case NodeType::StructDefinition: {
				auto const* const node = static_cast<StructDefinition const*>(inNode);
				str << indent(ind) << "<struct-definition>: " << endl;
				printTreeImpl(node, str, ind + 1, NodeType::StructDeclaration);
				printTreeImpl(node->body.get(), str, ind + 1);
				break;
			}
				
			case NodeType::VariableDeclaration: {
				auto const* const node = static_cast<VariableDeclaration const*>(inNode);
				str << indent(ind) << "<variable-declaration> " << node->name() << ": " << (node->declTypename.empty() ? "<deduce-type>" : node->declTypename.id);
				str << " [" << (node->isConstant ? "const" : "mutable") << "]" << endl;
				if (node->initExpression.get()) {
					printTreeImpl(node->initExpression.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::ExpressionStatement: {
				auto const* const node = static_cast<ExpressionStatement const*>(inNode);
				str << indent(ind) << "<expression-statement>" << endl;
				printTreeImpl(node->expression.get(), str, ind + 1);
				break;
			}
			case NodeType::ReturnStatement: {
				auto const* const node = static_cast<ReturnStatement const*>(inNode);
				str << indent(ind) << "<return-statement>" << endl;
				if (node->expression) {
					printTreeImpl(node->expression.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::IfStatement: {
				auto const* const node = static_cast<IfStatement const*>(inNode);
				str << indent(ind) << "<if-statement>" << endl;
				printTreeImpl(node->condition.get(), str, ind + 1);
				printTreeImpl(node->ifBlock.get(), str, ind + 1);
				if (node->elseBlock) {
					printTreeImpl(node->elseBlock.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::WhileStatement: {
				auto const* const node = static_cast<WhileStatement const*>(inNode);
				str << indent(ind) << "<while-statement>" << endl;
				printTreeImpl(node->condition.get(), str, ind + 1);
				printTreeImpl(node->block.get(), str, ind + 1);
				break;
			}
				
			case NodeType::Identifier: {
				auto const* const node = static_cast<Identifier const*>(inNode);
				str << indent(ind) << "<identifier>: " << node->value() << endl;
				break;
			}
				
			case NodeType::IntegerLiteral: {
				auto const* const node = static_cast<IntegerLiteral const*>(inNode);
				str << indent(ind) << "<integer-literal>: " << node->value << endl;
				break;
			}
				
			case NodeType::StringLiteral: {
				auto const* const node = static_cast<StringLiteral const*>(inNode);
				str << indent(ind) << "<string-literal>: " << '"' << node->value << '"' << endl;
				break;
			}
				
			case NodeType::UnaryPrefixExpression: {
				auto const* const node = static_cast<UnaryPrefixExpression const*>(inNode);
				str << indent(ind) << "<unary-prefix-expression> : " << '"' << toString(node->op) << '"' << endl;
				printTreeImpl(node->operand.get(), str, ind + 1);
				break;
			}
				
			case NodeType::BinaryExpression: {
				auto const* const node = static_cast<BinaryExpression const*>(inNode);
				str << indent(ind) << "<binary-expression> : " << '"' << toString(node->op) << '"' << endl;
				printTreeImpl(node->lhs.get(), str, ind + 1);
				printTreeImpl(node->rhs.get(), str, ind + 1);
				break;
			}
				
			case NodeType::MemberAccess: {
				auto const* const node = static_cast<MemberAccess const*>(inNode);
				str << indent(ind) << "<member-access> : " << endl;
				printTreeImpl(node->object.get(), str, ind + 1);
				str << indent(ind + 1) << "<identifier>: " << node->memberName() << endl;
				break;
			}
				
			case NodeType::Conditional: {
				auto const* const node = static_cast<Conditional const*>(inNode);
				str << indent(ind) << "<conditional>" << endl;
				printTreeImpl(node->condition.get(), str, ind + 1);
				printTreeImpl(node->ifExpr.get(), str, ind + 1);
				printTreeImpl(node->elseExpr.get(), str, ind + 1);
				break;
			}
				
			case NodeType::FunctionCall: {
				auto const* const node = static_cast<FunctionCall const*>(inNode);
				str << indent(ind) << "<function-call>" << endl;
				printTreeImpl(node->object.get(), str, ind + 1);
				for (auto& arg: node->arguments) {
					printTreeImpl(arg.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::Subscript: {
				auto const* const node = static_cast<Subscript const*>(inNode);
				str << indent(ind) << "<subscript>" << endl;
				printTreeImpl(node->object.get(), str, ind + 1);
				for (auto& arg: node->arguments) {
					printTreeImpl(arg.get(), str, ind + 1);
				}
				break;
			}
				
			case NodeType::_count:
				SC_DEBUGFAIL();
		}
	}
	
	
}
