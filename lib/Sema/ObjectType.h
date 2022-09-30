#ifndef SCATHA_SEMA_TYPE_H_
#define SCATHA_SEMA_TYPE_H_

#include "Basic/Basic.h"

#include "Sema/Scope.h"
#include "Sema/Variable.h"
#include "Sema/FunctionSignature.h"

namespace scatha::sema {
	
	class ObjectType: public Scope {
	public:
		explicit ObjectType(std::string name, SymbolID typeID, Scope* parentScope, size_t size = -1, size_t align = -1, bool isBuiltin = false):
			Scope(ScopeKind::Object, std::move(name), typeID, parentScope),
			_size(size),
			_align(align),
			_isBuiltin(isBuiltin)
		{}
		
		TypeID symbolID() const { return TypeID(EntityBase::symbolID()); }
		
		size_t size() const { return _size; }
		size_t align() const { return _align; }
		bool isBuiltin() const { return _isBuiltin; }
		
		void setSize(size_t value) { _size = value; }
		void setAlign(size_t value) { _align = value; }
		
	public:
		size_t _size;
		size_t _align;
		bool _isBuiltin;
	};
	
}

#endif // SCATHA_SEMA_TYPE_H_

