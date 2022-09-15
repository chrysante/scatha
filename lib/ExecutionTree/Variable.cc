#include "Variable.h"

#include <cstdlib>

namespace scatha::execution {
	
	Variable::Variable(Type const& type):
		type(type),
		bufferPtr(type.size() <= localBufferSize ? &localBuffer : std::malloc(type.size()))
	{}
	
}
