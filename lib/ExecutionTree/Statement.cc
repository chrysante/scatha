#include "Statement.h"

#include "Function.h"

namespace scatha::execution {

	ReturnStatement::ReturnStatement(Function& function, ExpressionNode const& expr):
		expression(expr),
		function(&function)
	{}

	void ReturnStatement::execute() {
		expression.eval(function->out);
		function->jumpBuffer.jump();
	}
	
}
