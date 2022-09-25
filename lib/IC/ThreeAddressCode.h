#ifndef SCATHA_IC_THREEADDRESSCODE_H_
#define SCATHA_IC_THREEADDRESSCODE_H_

#include <variant>

#include <utl/vector.hpp>

#include "IC/ThreeAddressStatement.h"

namespace scatha::ic {
	
	// TODO: Remove this
	struct TacLineCase {
		static constexpr size_t ThreeAddressStatement = 0;
		static constexpr size_t Label = 1;
		static constexpr size_t FunctionLabel = 2;
	};
	
	using TacLineVariant = std::variant<ThreeAddressStatement, Label, FunctionLabel>;
	
	struct TacLine: TacLineVariant {
		using TacLineVariant::TacLineVariant;
		
		bool isTas() const { return index() == 0; }
		bool isLabel() const { return index() == 1; }
		bool isFunctionLabel() const { return index() == 2; }
		
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

