#ifndef SCATHA_SEMA_TYPE_H_
#define SCATHA_SEMA_TYPE_H_

#include "Basic/Basic.h"

#include "Sema/Exp/Scope.h"
#include "Sema/Exp/Variable.h"
#include "Sema/Exp/FunctionSignature.h"

namespace scatha::sema::exp {
	
	class ObjectType: public Scope {
	public:
		explicit ObjectType(std::string name, SymbolID typeID, Scope* parentScope, size_t size = -1, size_t align = -1):
			Scope(ScopeKind::Object, std::move(name), typeID, parentScope),
			_size(size),
			_align(align)
		{}
		
		TypeID symbolID() const { return TypeID(EntityBase::symbolID()); }
		
		size_t size() const { return _size; }
		size_t align() const { return _align; }
		
		void setSize(size_t value) { _size = value; }
		void setAlign(size_t value) { _align = value; }
		
	public:
		size_t _size;
		size_t _align;
	};
	
}

#endif // SCATHA_SEMA_TYPE_H_

