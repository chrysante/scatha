#ifndef SCATHA_SEMA_SEMANTICELEMENTS_H_
#define SCATHA_SEMA_SEMANTICELEMENTS_H_

#include <algorithm>
#include <functional> // for std::hash
#include <utility>
#include <string>
#include <span>
#include <iosfwd>

#include <utl/common.hpp>
#include <utl/hashmap.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::sema {

	/**
	 * Category of a name. A name cannot refer to more than one category.
	 */
	enum class SymbolCategory: u32 {
		None      = 0,
		Type      = 1 << 0,
		Function  = 1 << 1,
		Variable  = 1 << 2,
		Namespace = 1 << 3
	};
	
	UTL_ENUM_OPERATORS(SymbolCategory);
	
	std::string_view toString(SymbolCategory);
	
	enum class TypeID: u64 { Invalid = 0 };
	
	/**
	 * ID of a name in the program. Used to quickly lookup named elements such as types, functions or variables
	 */
	class SCATHA(API) SymbolID {
	public:
		SymbolID() = default;
		constexpr explicit SymbolID(u64 id, SymbolCategory category):
			_id(id),
			_category(category)
		{}
		
		u64 id() const { return _id; }
		TypeID toTypeID() const { return TypeID(_id);  }
		SymbolCategory category() const { return _category; }
	
		bool operator==(SymbolID const& rhs) const { return id() == rhs.id(); }
		
		explicit operator bool() const { return id() != 0; }
		
	private:
		u64 _id{};
		SymbolCategory _category{};
	};
	
	inline constexpr auto invalidSymbolID = SymbolID{};
	
	// This could be marked 'gnu::pure' (not 'gnu::const' though)
	/**
	 Computes \p TypeID of a function type.
	 
	 - parameter \p returnType: TypeID of the return type.
	 - parameter \p argumentTypes: TypeIDs of the arguments.
	 - returns: TypeID of the function type.
	 
	 # Notes: #
	 1. Computes a hash of the argument TypeIDs and the return TypeID. 64 bits should hopefully be enugh to not have any collisions in the program. Collisions would be detected by the symbol table, they would however stop the compilation of a valid program.
	 */
	SCATHA(API) TypeID computeFunctionTypeID(TypeID returnType, std::span<TypeID const> argumentTypes);
	
	/**
	 Verifies that \p functionType matches the signature described by \p returnType and \p argumentTypes
	 Throws if the signatures don't match.
	 */
	void functionTypeVerifyEqual(Token const&,
								 struct TypeEx const& functionType,
								 TypeID returnType,
								 std::span<TypeID const> argumentTypes);
		
	/**
	 * class \p TypeEx
	 * Represents a type in the language. Types can be user defined.
	 * Extends \p Type with additional information.
	 */
	struct SCATHA(API) TypeEx {
		friend class SymbolTable;
		
	public:
		static constexpr std::string_view elementName() { return "Type"; }
		
	public:
		explicit TypeEx(std::string name, TypeID id, size_t size, size_t align);
		explicit TypeEx(TypeID returnType, std::span<TypeID const> argumentTypes, TypeID id);
		TypeEx(TypeEx const&);
		~TypeEx();
		
		TypeEx& operator=(TypeEx const&);
		
		size_t size() const { return _size; }
		size_t align() const { return _align; }
	
		TypeID id() const { return _id; }
		std::string_view name() const { return _name; }
		
		
		bool isFunctionType() const { return _isFunctionType; }
		bool isBuiltin() const { return _isBuiltin; }
		
		/// These may only be called if \p isFunctionType() returns true
		TypeID returnType() const { return _returnType; }
		size_t argumentCount() const { return _argumentTypes.size(); }
		std::span<TypeID const> argumentTypes() const { return _argumentTypes; }
		TypeID argumentType(size_t index) const { return _argumentTypes[index]; }
		
		friend bool operator==(TypeEx const&, TypeEx const&);
		
	private:
		struct PrivateCtorTag{};
		explicit TypeEx(PrivateCtorTag, auto&&);
		
	private:
		TypeID _id;
		
		u16 _size = 0;
		u16 _align = 0;
		
		bool _isFunctionType : 1 = false;
		bool _isBuiltin      : 1 = false;
		
		// Function types are not named so it is fine to put this in a union
		union {
			std::string _name;
			struct {
				TypeID _returnType;
				utl::small_vector<TypeID, 6> _argumentTypes;
			};
		};
	};
	
	/**
	 * class \p Function
	 * Represents a function in the language. Functions can be user defined.
	 * Functions have a name and a type.
	 */
	struct Function {
		static constexpr std::string_view elementName() { return "Function"; }
		
		explicit Function(std::string name, SymbolID symbolID, TypeID typeID):
			_name(name),
			_symbolID(symbolID),
			_typeID(typeID)
		{}
		
		std::string_view name() const { return _name; }
		SymbolID symbolID() const { return _symbolID; }
		TypeID typeID() const { return _typeID; }
		
	private:
		std::string _name;
		SymbolID _symbolID;
		TypeID _typeID = TypeID::Invalid;
	};
	
	/**
	 * class \p Variable
	 * Represents a variable in the language.
	 * Variables have a name and a type.
	 */
	struct Variable {
		static constexpr std::string_view elementName() { return "Variable"; }
		
		explicit Variable(std::string name, SymbolID symbolID, TypeID typeID, bool isConstant):
			_name(std::move(name)),
			_symbolID(symbolID),
			_typeID(typeID),
			_isConstant(isConstant)
		{}
		
		std::string_view name() const { return _name; }
		SymbolID symbolID() const { return _symbolID; }
		TypeID typeID() const { return _typeID; }
		bool isConstant() const { return _isConstant; }
		
	private:
		std::string _name;
		SymbolID _symbolID;
		TypeID _typeID;
		bool _isConstant: 1;
	};
	
	/**
	 * class template \p ElementTable
	 * Thin wrapper around a hashmap mapping \p SymbolID to \p Type \p Function or \p Variable
	 */
	template <typename T>
	class ElementTable {
	public:
		T& get(u64 id) {
			return utl::as_mutable(utl::as_const(*this).get(id));
		}
		
		T const& get(u64 id) const {
			auto* result = tryGet(id);
			SC_ASSERT(result, "Probably a bug");
			return *result;
		}
		
		T const* tryGet(u64 id) const {
			SC_EXPECT(id != 0, "Invalid TypeID");
			auto const itr = _elements.find(id);
			if (itr == _elements.end()) { return nullptr; }
			return itr->second.get();
		}
		
		template <typename... Args>
		std::pair<T*, bool> emplace(u64 id, Args&&... args) {
			auto [itr, success] = _elements.insert({ id, std::make_unique<T>(std::forward<Args>(args)...) });
			return { itr->second.get(), success };
		}
		
	private:
		utl::hashmap<u64, std::unique_ptr<T>> _elements;
	};
	
}

template <>
struct std::hash<scatha::sema::SymbolID> {
	std::size_t operator()(scatha::sema::SymbolID id) const { return std::hash<scatha::u64>{}(id.id()); }
};

#endif // SCATHA_SEMA_SEMANTICELEMENTS_H_

