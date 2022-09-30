#ifndef SCATHA_SEMA_SYMBOLID_H_
#define SCATHA_SEMA_SYMBOLID_H_

#include <functional>
#include <iosfwd>

#include "Basic/Basic.h"

namespace scatha::sema::exp {
	
	class SymbolID {
	public:
		constexpr SymbolID() = default;
		constexpr explicit SymbolID(u64 rawValue): _value(rawValue) {}
		
		static SymbolID const Invalid;
		
		constexpr u64 rawValue() const { return _value; }
		
		constexpr bool operator==(SymbolID const&) const = default;
		
		u64 hash() const;
		
	private:
		u64 _value;
	};
	
	inline SymbolID const SymbolID::Invalid = SymbolID(0);

	SCATHA(API) std::ostream& operator<<(std::ostream&, SymbolID);
	
	// Special kind of SymbolID
	struct TypeID: SymbolID {
		explicit TypeID(SymbolID id): SymbolID(id) {}
		using SymbolID::SymbolID;
		
		static TypeID const Invalid;
	};
	
	inline TypeID const TypeID::Invalid = TypeID(SymbolID::Invalid);
	
}

template <>
struct std::hash<scatha::sema::exp::SymbolID> {
	std::size_t operator()(scatha::sema::exp::SymbolID id) const { return std::hash<scatha::u64>{}(id.rawValue()); }
};

template <>
struct std::hash<scatha::sema::exp::TypeID> {
	std::size_t operator()(scatha::sema::exp::TypeID id) const { return std::hash<scatha::sema::exp::SymbolID>{}(id); }
};


#endif // SCATHA_SEMA_SYMBOLID_H_

