#ifndef SCATHA_AST_TRAVERSAL_H_
#define SCATHA_AST_TRAVERSAL_H_

#include "AST/AST.h"
#include "Basic/Basic.h"

namespace scatha::ast {
	namespace internal {
		template <typename, typename>
		struct TraversalContext;
		template <typename T>
		constexpr T* removeConst(T const* ptr) { return const_cast<T*>(ptr); }
	}
	
	template <typename Ctx>
	void traverse(AbstractSyntaxTree* node, Ctx ctx) {
		internal::TraversalContext _ctx{
			[&](auto const* node) {
				if constexpr (requires { ctx.pre(internal::removeConst(node)); }) {
					ctx.pre(internal::removeConst(node));
				}
			},
			[&](auto const* node) {
				if constexpr (requires { ctx.post(internal::removeConst(node)); }) {
					ctx.post(internal::removeConst(node));
				}
			}
		};
		_ctx.traverse(node);
	}
	template <typename Ctx>
	void traverse(AbstractSyntaxTree const* node, Ctx ctx) {
		internal::TraversalContext _ctx{
			[&](auto const* node) {
				if constexpr (requires(AbstractSyntaxTree const* node) { ctx.pre(node); }) {
					ctx.pre(node);
				}
			},
			[&](auto const* node) {
				if constexpr (requires(AbstractSyntaxTree const* node) { ctx.post(node); }) {
					ctx.post(node);
				}
			}
		};
		_ctx.traverse(node);
	}
}

namespace scatha::ast::internal {
	
	template <typename Pre, typename Post>
	struct TraversalContext {
		TraversalContext(Pre pre, Post post): pre(pre), post(post) {}
		
		void traverse(AbstractSyntaxTree const* node) { traverse(node, node->nodeType()); }
		void traverse(AbstractSyntaxTree const* inNode, NodeType type) {
			switch (type) {
				case NodeType::TranslationUnit: {
					auto const* const node = static_cast<TranslationUnit const*>(inNode);
					pre(node);
					for (auto& decl: node->declarations) {
						traverse(decl.get());
					}
					post(node);
					break;
				}
				case NodeType::Block: {
					auto const* const node = static_cast<Block const*>(inNode);
					pre(node);
					for (auto& s: node->statements) {
						traverse(s.get());
					}
					post(node);
					break;
				}
				case NodeType::FunctionDeclaration: {
					auto const* const node = static_cast<FunctionDeclaration const*>(inNode);
					pre(node);
					for (auto& p: node->parameters) {
						traverse(p.get());
					}
					post(node);
					break;
				}
				case NodeType::FunctionDefinition: {
					auto const* const node = static_cast<FunctionDefinition const*>(inNode);
					traverse(node, NodeType::FunctionDeclaration);
					pre(node);
					traverse(node->body.get());
					post(node);
					break;
				}
				case NodeType::StructDeclaration: {
					auto const* const node = static_cast<StructDeclaration const*>(inNode);
					pre(node);
					post(node);
					break;
				}
				case NodeType::StructDefinition: {
					auto const* const node = static_cast<StructDefinition const*>(inNode);
					traverse(node, NodeType::StructDeclaration);
					pre(node);
					traverse(node->body.get());
					post(node);
					break;
				}
				case NodeType::VariableDeclaration: {
					auto const* const node = static_cast<VariableDeclaration const*>(inNode);
					pre(node);
					if (node->initExpression != nullptr) {
						traverse(node->initExpression.get());
					}
					post(node);
					break;
				}
				case NodeType::ExpressionStatement: {
					auto const* const node = static_cast<ExpressionStatement const*>(inNode);
					pre(node);
					SC_ASSERT(node->expression != nullptr, "");
					traverse(node->expression.get());
					post(node);
					break;
				}
				case NodeType::ReturnStatement: {
					auto const* const node = static_cast<ReturnStatement const*>(inNode);
					pre(node);
					if (node->expression != nullptr) {
						traverse(node->expression.get());
					}
					post(node);
					break;
				}
				case NodeType::IfStatement: {
					auto const* const node = static_cast<IfStatement const*>(inNode);
					pre(node);
					traverse(node->condition.get());
					traverse(node->ifBlock.get());
					if (node->elseBlock != nullptr) {
						traverse(node->elseBlock.get());
					}
					post(node);
					break;
				}
				case NodeType::WhileStatement: {
					auto const* const node = static_cast<WhileStatement const*>(inNode);
					pre(node);
					traverse(node->condition.get());
					traverse(node->block.get());
					post(node);
					break;
				}
				case NodeType::Identifier: {
					auto const* const node = static_cast<Identifier const*>(inNode);
					pre(node);
					post(node);
					break;
				}
				case NodeType::IntegerLiteral: {
					auto const* const node = static_cast<IntegerLiteral const*>(inNode);
					pre(node);
					post(node);
					break;
				}
				case NodeType::FloatingPointLiteral: {
					auto const* const node = static_cast<FloatingPointLiteral const*>(inNode);
					pre(node);
					post(node);
					break;
				}
				case NodeType::StringLiteral: {
					auto const* const node = static_cast<StringLiteral const*>(inNode);
					pre(node);
					post(node);
					break;
				}
				case NodeType::UnaryPrefixExpression: {
					auto const* const node = static_cast<UnaryPrefixExpression const*>(inNode);
					pre(node);
					traverse(node->operand.get());
					post(node);
					break;
				}
				case NodeType::BinaryExpression: {
					auto const* const node = static_cast<BinaryExpression const*>(inNode);
					pre(node);
					traverse(node->lhs.get());
					traverse(node->rhs.get());
					post(node);
					break;
				}
				case NodeType::MemberAccess: {
					auto const* const node = static_cast<MemberAccess const*>(inNode);
					pre(node);
					traverse(node->object.get());
					post(node);
					break;
				}
				case NodeType::Conditional: {
					auto const* const node = static_cast<Conditional const*>(inNode);
					pre(node);
					traverse(node->condition.get());
					traverse(node->ifExpr.get());
					traverse(node->elseExpr.get());
					post(node);
					break;
				}
				case NodeType::FunctionCall: {
					auto const* const node = static_cast<FunctionCall const*>(inNode);
					pre(node);
					traverse(node->object.get());
					for (auto& arg: node->arguments) {
						traverse(arg.get());
					}
					post(node);
					break;
				}
				case NodeType::Subscript: {
					auto const* const node = static_cast<Subscript const*>(inNode);
					pre(node);
					traverse(node->object.get());
					for (auto& arg: node->arguments) {
						traverse(arg.get());
					}
					post(node);
					break;
				}
					
				SC_NO_DEFAULT_CASE();
			}
		}
		
		Pre pre;
		Post post;
	};
	
}

#endif // SCATHA_AST_TRAVERSAL_H_

