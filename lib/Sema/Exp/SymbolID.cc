#include "Sema/Exp/SymbolID.h"

#include <ostream>

namespace scatha::sema::exp {
	
	u64 SymbolID::hash() const {
		auto x = rawValue();
		x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
		x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
		x =  x ^ (x >> 31);
		return x;
	}
	
	std::ostream& operator<<(std::ostream& str, SymbolID id) {
		auto const flags = str.flags();
		str << std::hex << id.rawValue();
		str.flags(flags);
		return str;
	}
	
}
