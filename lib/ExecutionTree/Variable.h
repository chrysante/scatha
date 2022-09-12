#pragma once

#ifndef SCATHA_EXECUTIONTREE_VARIABLE_H_
#define SCATHA_EXECUTIONTREE_VARIABLE_H_

#include "Basic/Basic.h"

#include "Common/Type.h"

namespace scatha::execution {

	struct Variable {
		static constexpr size_t localBufferSize = 8;
		
		explicit Variable(scatha::Type const&);
		
		scatha::Type type;
		void* bufferPtr;
		u8 localBuffer[localBufferSize];
	};
	
}


#endif // SCATHA_EXECUTIONTREE_VARIABLE_H_
