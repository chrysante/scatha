#pragma once

#ifndef SCATHA_COMMON_TYPE_H_
#define SCATHA_COMMON_TYPE_H_

#include <string>
#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Name.h"

namespace scatha {
	
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
	
	struct TypeEx: Type {
		static constexpr std::string_view elementName() { return "Type"; }
	protected:
		friend struct TypeTable;
		
	public:
		explicit TypeEx(std::string name, TypeID id, size_t size);
		explicit TypeEx(TypeID returnType, std::span<TypeID const> argumentTypes, TypeID id);
		
	private:
		struct PrivateCtorTag{};
		explicit TypeEx(PrivateCtorTag, auto&&);
		
	public:
		TypeEx(TypeEx const&);
		~TypeEx();
		
		TypeEx& operator=(TypeEx const&);
		
	public:
		TypeID id() const { return _id; }
		std::string_view name() const { return _name; }
		
		bool isFunctionType() const { return _isFunctionType; }
		TypeID returnType() const { return _returnType; }
		std::span<TypeID const> argumentTypes() const { return _argumentTypes; }
		
		friend bool operator==(TypeEx const&, TypeEx const&);
		
	private:
		TypeID _id: sizeof(TypeID) * CHAR_BIT - 1;
		bool _isFunctionType: 1 = false;
		
		union {
			std::string _name;
			struct {
				TypeID _returnType;
				utl::small_vector<TypeID, 6> _argumentTypes;
			};
		};
	};
	
}


#endif // SCATHA_EXECUTIONTREE_TYPE_H_
