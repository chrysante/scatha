#ifndef SCATHA_EXECUTIONTREE_TYPE_H_
#define SCATHA_EXECUTIONTREE_TYPE_H_

#include "Basic/Basic.h"

namespace scatha::execution {
	
	struct Type {
	protected:
		friend struct TypeTable;
		friend struct IdentifierTable;
		explicit Type(size_t size): _size(size) {}
		
	public:
		Type() = default;
		size_t size() const { return _size; }
		size_t align() const { return _align; }
		
	private:
		u16 _size = 0;
		u16 _align = 0;
	};
	
}

#endif // SCATHA_EXECUTIONTREE_TYPE_H_

