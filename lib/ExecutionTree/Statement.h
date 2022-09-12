#pragma once

#ifndef SCATHA_EXECUTIONTREE_STATEMENT_H_
#define SCATHA_EXECUTIONTREE_STATEMENT_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"

#include "ExecutionTree/Expression.h"
#include "ExecutionTree/Variable.h"

namespace scatha::execution {

	struct Function;
	
	struct Statement {
		virtual void execute() = 0;
	};
	
	struct VariableDeclaration: Statement {
		VariableDeclaration(Variable const& var, ExpressionNode const& initExpr) :variable(var), initExpression(initExpr) {}
		
		void execute() override {
			initExpression.eval(variable.bufferPtr);
		}
		
		Variable variable;
		ExpressionNode initExpression;
	};
	
	struct ExpressionStatement: Statement {
		void execute() override {
			void* buffer = alloca(expression.type.size());
			expression.eval(buffer);
			(void)buffer;
		}
		
		ExpressionNode expression;
	};

	struct StatementBlock: Statement {
		void execute() override {
			for (auto s: statements) {
				s->execute();
			}
		}
		
		void add(Statement* s) { statements.push_back(s); }
		
		utl::small_vector<Statement*> statements;
	};
	
	/// MARK: Control Flow
	struct ControlFlowStatement: Statement {};

	struct ReturnStatement: ControlFlowStatement {
		explicit ReturnStatement(Function& function, ExpressionNode const& expr);
		
		[[noreturn]] void execute() override;
		
		ExpressionNode expression;
		Function* function;
	};
	
	struct IfStatement: ControlFlowStatement {
		void execute() override {
			bool cond;
			condition.eval(&cond);
			if (cond) {
				ifBlock.execute();
			}
			else {
				thenBlock.execute();
			}
		}
		
		ExpressionNode condition;
		StatementBlock ifBlock, thenBlock;
	};
	
}


#endif // SCATHA_EXECUTIONTREE_STATEMENT_H_
