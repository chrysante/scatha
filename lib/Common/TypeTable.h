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
		
		TypeEx& findByID(u32 id) {
			return utl::as_mutable(utl::as_const(*this).findByID(id));
		}
		TypeEx const& findByID(u32 id) const;
		
		void add(std::string name, size_t size);
		void addFunctionType(u32 returnType, std::span<u32 const> argumentTypes);
		
	private:
		utl::hashmap<std::string, size_t> _nameMap;
		utl::hashmap<u32, size_t> _idMap;
		utl::vector<TypeEx> _types;
		
		u32 _currentID = 0;
	};
	
}


#endif // SCATHA_EXECUTIONTREE_TYPETABLE_H_
