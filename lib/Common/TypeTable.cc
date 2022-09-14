#include "Common/TypeTable.h"

#include <string>

#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Basic/Basic.h"

namespace scatha {
	
	TypeTable::TypeTable() {
		_void   = add("void", 0);
		_bool   = add("bool", 1);
		_int    = add("int", 8);
		_float  = add("float", 8);
		_string = add("string", sizeof(std::string));
	}
	
	static TypeEx const& findImpl(auto const& map, auto& types, auto value, auto _throw) {
		auto const itr = map.find(value);
		if (itr == map.end()) {
			_throw();
		}
		return types[itr->second];
	}
	
	TypeEx const& TypeTable::findByName(std::string_view name) const {
		return findImpl(_nameMap, _types, std::string(name),
						[&]{ throw std::runtime_error("Can't find type \"" + std::string(name) + "\""); });
	}
	
	TypeEx const& TypeTable::findByID(TypeID id) const {
		SC_EXPECT(id != TypeID::Invalid, "Invalid TypeID");
		return findImpl(_idMap, _types, id,
						[&]{ throw std::runtime_error("Can't find type with ID " + std::to_string(utl::to_underlying(id))); });
	}
	
	TypeID TypeTable::add(std::string name, size_t size) {
		size_t const index = _types.size();
		auto const id = TypeID(++_currentID);
		
		_nameMap.insert({ name, index });
		_idMap.insert({ id, index });
		_types.push_back(TypeEx(std::move(name), id, size));
		
		return id;
	}

	TypeID TypeTable::addFunctionType(TypeID returnType, std::span<TypeID const> argumentTypes) {
		size_t const index = _types.size();
		auto const id = TypeID(++_currentID);
		
		_idMap.insert({ id, index });
		_types.push_back(TypeEx(returnType, argumentTypes, id));
		
		return id;
	}
	
}
