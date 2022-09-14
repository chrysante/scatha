#include "AST/AST.h"

#include <ostream>

#include <utl/utility.hpp>

#include "AST/Expression.h"

namespace scatha::ast {

	VariableDeclaration::VariableDeclaration(std::string name):
		Declaration(NodeType::VariableDeclaration, std::move(name))
	{}
	
	ExpressionStatement::ExpressionStatement(UniquePtr<Expression> expression):
		Statement(NodeType::ExpressionStatement),
		expression(std::move(expression))
	{}
	ExpressionStatement::~ExpressionStatement() = default;
	
	ReturnStatement::ReturnStatement(UniquePtr<Expression> expression):
		ControlFlowStatement(NodeType::ReturnStatement),
		expression(std::move(expression))
	{}
	
	IfStatement::IfStatement(UniquePtr<Expression> condition, UniquePtr<Block> ifBlock, UniquePtr<Block> elseBlock):
		ControlFlowStatement(NodeType::IfStatement),
		condition(std::move(condition)),
		ifBlock(std::move(ifBlock)),
		elseBlock(std::move(elseBlock))
	{}
	
	WhileStatement::WhileStatement(UniquePtr<Expression> condition, UniquePtr<Block> block):
		ControlFlowStatement(NodeType::WhileStatement),
		condition(std::move(condition)),
		block(std::move(block))
	{}
	
}
