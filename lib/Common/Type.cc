#include "Common/Type.h"

#include <memory>

namespace scatha {
	
	TypeEx::TypeEx(std::string name, TypeID id, size_t size):
		Type(size),
		_id(id),
		_isFunctionType{ false },
		_name(std::move(name))
	{}
	
	TypeEx::TypeEx(TypeID returnType, std::span<TypeID const> argumentTypes, TypeID id):
		Type(0),
		_id(id),
		_isFunctionType{ true },
		_returnType { returnType },
		_argumentTypes{}
	{
		_argumentTypes.reserve(argumentTypes.size());
		for (auto a: argumentTypes) {
			_argumentTypes.push_back(a);
		}
	}
	
	TypeEx::TypeEx(TypeEx const& rhs):
		Type(rhs),
		_id(rhs._id),
		_isFunctionType(rhs._isFunctionType)
	{
		if (rhs.isFunctionType()) {
			_returnType = rhs._returnType;
			std::construct_at(&_argumentTypes, rhs._argumentTypes);
		}
		else {
			std::construct_at(&_name, rhs._name);
		}
	}
	
	TypeEx::~TypeEx() {
		if (isFunctionType()) {
			std::destroy_at(&_argumentTypes);
		}
		else {
			std::destroy_at(&_name);
		}
	}
	
	TypeEx& TypeEx::operator=(TypeEx const& rhs) {
		std::destroy_at(this);
		std::construct_at(this, rhs);
		return *this;
	}

	bool operator==(TypeEx const& a, TypeEx const& b) {
		if (a._isFunctionType != b._isFunctionType) {
			return false;
		}
		if (!a._isFunctionType) {
			return a._id == b._id;
		}
		else {
			if (a._argumentTypes.size() != b._argumentTypes.size()) {
				return false;
			}
			for (size_t i = 0; i < a._argumentTypes.size(); ++i) {
				if (a._argumentTypes[i] != a._argumentTypes[i]) {
					return false;
				}
			}
			return true;
		}
	}
	
}
