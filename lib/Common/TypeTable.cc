#include "Common/TypeTable.h"

#include <utl/strcat.hpp>

namespace scatha {
	
	TypeTable::TypeTable() {
		add("void", 0);
		add("bool", 1);
		add("int", 8);
		add("float", 8);
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
	
	TypeEx const& TypeTable::findByID(u32 id) const {
		return findImpl(_idMap, _types, id,
						[&]{ throw std::runtime_error("Can't find type with ID " + std::to_string(id)); });
	}
	
	void TypeTable::add(std::string name, size_t size) {
		size_t const index = _types.size();
		u32 const id = _currentID++;
		_nameMap.insert({ name, index });
		_idMap.insert({ id, index });
		_types.push_back(TypeEx(std::move(name), id, size));
	}

	void TypeTable::addFunctionType(u32 returnType, std::span<u32 const> argumentTypes) {
		size_t const index = _types.size();
		u32 const id = _currentID++;
		
		_idMap.insert({ id, index });
		_types.push_back(TypeEx(returnType, argumentTypes, id));
	}
	
}
