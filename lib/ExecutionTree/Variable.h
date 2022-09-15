#pragma once

#ifndef SCATHA_EXECUTIONTREE_VARIABLE_H_
#define SCATHA_EXECUTIONTREE_VARIABLE_H_

#include "Basic/Basic.h"
#include "ExecutionTree/Type.h"

namespace scatha::execution {

	struct Variable {
		static constexpr size_t localBufferSize = 8;
		
		explicit Variable(Type const&);
		
		Type type;
		void* bufferPtr;
		u8 localBuffer[localBufferSize];
	};
	
}


#endif // SCATHA_EXECUTIONTREE_VARIABLE_H_
