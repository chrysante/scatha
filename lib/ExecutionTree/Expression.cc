#include "ExecutionTree/Expression.h"

#include <cstring>

#include "ExecutionTree/Variable.h"

namespace scatha::execution {
	
	ExpressionNode variableReference(Variable const& var) {
		ExpressionNode result;
		result.type = var.type;
		result.dataPtr = var.bufferPtr;
		result.function = [](ExpressionNode& self, void* out, void const* in) {
			std::memcpy(out, self.dataPtr, self.type.size());
		};
		return result;
	}
	
}
