#pragma once

#ifndef SCATHA_COMMON_TYPETABLE_H_
#define SCATHA_COMMON_TYPETABLE_H_

#include <string>

#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Type.h"

namespace scatha {
	
	class TypeTable {
	public:
		TypeTable();
		
		TypeEx& findByName(std::string_view name) {
			return utl::as_mutable(utl::as_const(*this).findByName(name));
		}
		TypeEx const& findByName(std::string_view name) const;
		
		TypeEx& findByID(TypeID id) {
			return utl::as_mutable(utl::as_const(*this).findByID(id));
		}
		TypeEx const& findByID(TypeID id) const;
		
		TypeID add(std::string name, size_t size);
		TypeID addFunctionType(TypeID returnType, std::span<TypeID const> argumentTypes);
		
		TypeID Void() const  { return _void; }
		TypeID Bool() const  { return _bool; }
		TypeID Int() const   { return _int; }
		TypeID Float() const { return _float; }
		TypeID String() const { return _string; }
		
	private:
		utl::hashmap<std::string, size_t> _nameMap;
		utl::hashmap<TypeID, size_t> _idMap;
		utl::vector<TypeEx> _types;
		
		std::underlying_type_t<TypeID> _currentID = 0;
		
		TypeID _void{}, _bool{}, _int{}, _float, _string{};
	};
	
}


#endif // SCATHA_EXECUTIONTREE_TYPETABLE_H_
