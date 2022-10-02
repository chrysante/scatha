#ifndef SCATHA_SEMA_TYPE_H_
#define SCATHA_SEMA_TYPE_H_

#include "Basic/Basic.h"

#include "Sema/Scope.h"
#include "Sema/Variable.h"
#include "Sema/FunctionSignature.h"

namespace scatha::sema {
	
	size_t constexpr invalidSize = static_cast<size_t>(-1);
	
	class ObjectType: public Scope {
	public:
		explicit ObjectType(std::string name,
							SymbolID typeID,
							Scope* parentScope,
							size_t size = invalidSize,
							size_t align = invalidSize,
							bool isBuiltin = false):
			Scope(ScopeKind::Object, std::move(name), typeID, parentScope),
			_size(size),
			_align(align),
			_isBuiltin(isBuiltin)
		{}
		
		TypeID symbolID() const { return TypeID(EntityBase::symbolID()); }
		
		size_t size() const { return _size; }
		size_t align() const { return _align; }
		bool isBuiltin() const { return _isBuiltin; }
		
		bool isComplete() const;
		
		void setSize(size_t value) { _size = value; }
		void setAlign(size_t value) { _align = value; }
		
	public:
		size_t _size;
		size_t _align;
		bool _isBuiltin;
	};
	
}

#endif // SCATHA_SEMA_TYPE_H_

