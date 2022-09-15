#pragma once

#ifndef SCATHA_EXECUTIONTREE_FUNCTION_H_
#define SCATHA_EXECUTIONTREE_FUNCTION_H_

#include <utl/vector.hpp>

#include "ExecutionTree/Statement.h"
#include "ExecutionTree/JumpBuffer.h"
#include "ExecutionTree/Type.h"
#include "ExecutionTree/Variable.h"

namespace scatha::execution {

	struct Function {
		void invoke(void* out);
		
		Type returnType;
		void* out;
		utl::small_vector<Variable> arguments;
		StatementBlock statements;
		JumpBuffer jumpBuffer;
	};
	
	inline void Function::invoke(void* out) {
		this->out = out;
		if (jumpBuffer.set()) {
			statements.execute();
		}
	}
	
}

#endif // SCATHA_EXECUTIONTREE_FUNCTION_H_


