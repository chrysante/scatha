#ifndef SCATHA_IC_THREEADDRESSCODE_H_
#define SCATHA_IC_THREEADDRESSCODE_H_

#include <variant>

#include <utl/vector.hpp>

#include "IC/ThreeAddressStatement.h"

namespace scatha::ic {
	
	using TacLineVariant = std::variant<ThreeAddressStatement, Label, FunctionLabel>;
	
	struct TacLine: TacLineVariant {
		using TacLineVariant::TacLineVariant;
		
		ThreeAddressStatement& asTas() { return utl::as_mutable(utl::as_const(*this).asTas()); }
		ThreeAddressStatement const& asTas() const { return std::get<ThreeAddressStatement>(*this); }
		
		Label& asLabel() { return utl::as_mutable(utl::as_const(*this).asLabel()); }
		Label const& asLabel() const { return std::get<Label>(*this); }
		
		FunctionLabel& asFunctionLabel() { return utl::as_mutable(utl::as_const(*this).asFunctionLabel()); }
		FunctionLabel const& asFunctionLabel() const { return std::get<FunctionLabel>(*this); }
	};
	
	struct ThreeAddressCode {
		utl::vector<TacLine> statements;
	};
	
}

#endif // SCATHA_IC_THREEADDRESSCODE_H_

