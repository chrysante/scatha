#include "Common/TypeTable.h"

#include <string>

#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/hash.hpp>

#include "Basic/Basic.h"

namespace scatha {
	
	static u64 hashOne(u64 x) {
		x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
		x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
		x =  x ^ (x >> 31);
		return x;
	}
	
	static TypeID typeHash(std::span<TypeID const> types) {
		auto r = utl::transform_range(types.begin(), types.end(), [](TypeID x) {
			return hashOne(utl::to_underlying(x));
		});
		return TypeID(utl::hash_combine_range(r.begin(), r.end()));
	}
	
	static void functionTypeVerifyEqual(TypeEx const& type, TypeID returnType, std::span<TypeID const> argumentTypes) {
		SC_ASSERT(type.isFunctionType(), "");
		SC_ASSERT(type.returnType() == returnType, "");
		SC_ASSERT(type.argumentTypes().size() == argumentTypes.size(), "");
		for (size_t i = 0; i < argumentTypes.size(); ++i) {
			auto const lhs = type.argumentTypes();
			auto const rhs = argumentTypes;
			SC_ASSERT(lhs[i] == rhs[i], "");
		}
	}
	
	TypeID computeFunctionTypeID(TypeID returnType, std::span<TypeID const> argumentTypes) {
		TypeID arr[2]{ returnType, typeHash(argumentTypes) };
		return typeHash(arr);
	}
	
	
	TypeTable::TypeTable() {
		_void   = addObjectType("void", 0).id();
		_bool   = addObjectType("bool", 1).id();
		_int    = addObjectType("int", 8).id();
		_float  = addObjectType("float", 8).id();
		_string = addObjectType("string", sizeof(std::string)).id();
	}

	static TypeEx const& findImpl(auto const& map, auto& types, auto value, auto _throw) {
		auto const itr = map.find(value);
		if (itr == map.end()) {
			_throw();
		}
		return types[itr->second];
	}

	TypeEx const& TypeTable::findTypeByName(std::string_view name) const {
		return findImpl(_nameMap, _types, std::string(name),
						[&]{ throw std::runtime_error("Can't find type \"" + std::string(name) + "\""); });
	}

	TypeEx const& TypeTable::findTypeByID(TypeID id) const {
		SC_EXPECT(id != TypeID::Invalid, "Invalid TypeID");
		return findImpl(_idMap, _types, id,
						[&]{ throw std::runtime_error("Can't find type with ID " + std::to_string(utl::to_underlying(id))); });
	}

	TypeEx& TypeTable::addObjectType(std::string name, size_t size) {
		size_t const index = _types.size();
		auto const [itr, success] = _nameMap.insert({ name, index });

		if (!success) {
			return _types[itr->second];
		}

		auto const id = TypeID(++_currentID);
		bool const idInsertSuccess = _idMap.insert({ id, index }).second;
		SC_ASSERT(idInsertSuccess, "");
		_types.push_back(TypeEx(std::move(name), id, size));
		return _types.back();
	}

	TypeEx& TypeTable::addFunctionType(TypeID returnType, std::span<TypeID const> argumentTypes) {
		auto const id = computeFunctionTypeID(returnType, argumentTypes);

		if (auto itr = _idMap.find(id); itr != _idMap.end()) {
			size_t const index = itr->second;
			auto& type = _types[index];
			functionTypeVerifyEqual(type, returnType, argumentTypes);
			return type;
		}

		size_t const index = _types.size();
		bool const idInsertSuccess = _idMap.insert({ id, index }).second;
		SC_ASSERT(idInsertSuccess, "");
		_types.push_back(TypeEx(returnType, argumentTypes, id));
		return _types.back();
	}
	
}
