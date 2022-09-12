#pragma once

#ifndef SCATHA_COMMON_TYPE_H_
#define SCATHA_COMMON_TYPE_H_

#include <string>
#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"

namespace scatha {
	
	struct Type {
	protected:
		friend struct TypeTable;
		explicit Type(size_t size): _size(size) {}
		
	public:
		Type() = default;
		size_t size() const { return _size; }
		
	private:
		u16 _size = 0;
	};
	
	struct TypeEx: Type {
	protected:
		friend struct TypeTable;
		
		explicit TypeEx(std::string name, u32 id, size_t size);
		explicit TypeEx(u32 returnType, std::span<u32 const> argumentTypes, u32 id);
		
	private:
		struct PrivateCtorTag{};
		explicit TypeEx(PrivateCtorTag, auto&&);
		
	public:
		TypeEx(TypeEx const&);
		~TypeEx();
		
		TypeEx& operator=(TypeEx const&);
		
	public:
		u32 id() const { return _id; }
		std::string_view name() const { return _name; }
		
		bool isFunctionType() const { return _isFunctionType; }
		u32 returnType() const { return _returnType; }
		std::span<u32 const> argumentTypes() const { return _argumentTypes; }
		
		friend bool operator==(TypeEx const&, TypeEx const&);
		
	private:
		u32 _id: 31;
		bool _isFunctionType: 1 = false;
		
		union {
			std::string _name;
			struct {
				u32 _returnType;
				utl::small_vector<u32, 6> _argumentTypes;
			};
		};
	};
	
}


#endif // SCATHA_EXECUTIONTREE_TYPE_H_
