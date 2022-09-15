#pragma once

#ifndef SCATHA_EXECUTIONTREE_EXPRESSION_H_
#define SCATHA_EXECUTIONTREE_EXPRESSION_H_

#include <alloca.h>

#include "Basic/Basic.h"
#include "ExecutionTree/Type.h"

namespace scatha::execution {

	struct Variable;
	
	struct ExpressionNode {
		using FunctionPtr = void(*)(ExpressionNode& self, void* out, void const* in);
		
		void eval(void* out);
		
		i8 numArgs = 0;
		Type type;
		union {
			ExpressionNode* children[3];
			void* dataPtr;
		};
		FunctionPtr function = nullptr;
	};
	
	inline void ExpressionNode::eval(void* out) {
		u8* const argBuffer = (u8*)alloca(8 * numArgs);
		for (int i = 0; i < numArgs; ++i) {
			children[i]->eval(argBuffer + 8 * i);
		}
		function(*this, out, argBuffer);
	}

	ExpressionNode makeVariableReference(Variable const&);
	
}


#endif // SCATHA_EXECUTIONTREE_EXPRESSION_H_
