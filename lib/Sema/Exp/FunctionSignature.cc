#include "Sema/Exp/FunctionSignature.h"

#include <utl/hash.hpp>
#include <utl/utility.hpp>

namespace scatha::sema::exp {
	
	u64 FunctionSignature::hashArguments(std::span<TypeID const> types) {
		auto r = utl::transform_range(types.begin(), types.end(), [](TypeID x) {
			return x.hash();
		});
		return utl::hash_combine_range(r.begin(), r.end());
	}
	
	u64 FunctionSignature::computeTypeHash(TypeID returnTypeID,  u64 argumentHash) {
		return utl::hash_combine(returnTypeID.hash(), argumentHash);
	}
	
}
