#include "Variable.h"

namespace scatha::execution {
	
	Variable::Variable(scatha::Type const& type):
		type(type),
		bufferPtr(type.size() <= localBufferSize ? &localBuffer : std::malloc(type.size()))
	{}
	
}
