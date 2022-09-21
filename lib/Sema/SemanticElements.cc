#include "Sema/SemanticElements.h"

#include "Sema/SemanticIssue.h"

namespace scatha::sema {

	/// MARK: SymbolID
	std::string_view toString(SymbolCategory c) {
		using enum SymbolCategory;
		switch (c) {
			case None:
				return "None";
			case Type:
				return "Type";
			case Function:
				return "Function";
			case Variable:
				return "Variable";
			case Namespace:
				return "Namespace";
		}
	}
	
	/// MARK: computeFunctionTypeID
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
	
	TypeID computeFunctionTypeID(TypeID returnType, std::span<TypeID const> argumentTypes) {
		TypeID arr[2]{ returnType, typeHash(argumentTypes) };
		return typeHash(arr);
	}
	
	/// MARK: functionTypeVerifyEqual
	void functionTypeVerifyEqual(Token const& functionName, TypeEx const& type, TypeID returnType, std::span<TypeID const> argumentTypes) {
		SC_ASSERT(type.isFunctionType(), "Passed type is not even a function type");
		
		if (type.returnType() != returnType) {
			throw InvalidRedeclaration(functionName, type);
		}
		if (type.argumentTypes().size() != argumentTypes.size()) {
			throw InvalidRedeclaration(functionName, type);
		}
		for (size_t i = 0; i < argumentTypes.size(); ++i) {
			auto const lhs = type.argumentTypes();
			auto const rhs = argumentTypes;
			if (lhs[i] != rhs[i]) {
				throw InvalidRedeclaration(functionName, type);
			}
		}
	}
	
	/// MARK: TypeEx
	TypeEx::TypeEx(std::string name, TypeID id, size_t size, size_t align):
		_id(id),
		_size(size),
		_align(align),
		_isFunctionType{ false },
		_name(std::move(name))
	{}
	
	TypeEx::TypeEx(TypeID returnType, std::span<TypeID const> argumentTypes, TypeID id):
		_id(id),
		_isFunctionType{ true },
		_returnType { returnType },
		_argumentTypes{}
	{
		if (_id == TypeID::Invalid) {
			_id = computeFunctionTypeID(returnType, argumentTypes);
		}
		else {
			SC_ASSERT(_id == computeFunctionTypeID(returnType, argumentTypes), "Invalid TypeID passed here");
		}
		_argumentTypes.reserve(argumentTypes.size());
		for (auto a: argumentTypes) {
			_argumentTypes.push_back(a);
		}
	}
	
	TypeEx::TypeEx(TypeEx const& rhs):
		_id(rhs._id),
		_size(rhs._size),
		_align(rhs._align),
		_isFunctionType(rhs._isFunctionType)
	{
		if (rhs.isFunctionType()) {
			std::construct_at(&_returnType, rhs._returnType);
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
