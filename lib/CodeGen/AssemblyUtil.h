#ifndef SCATHA_CODEGEN_LABEL_H_
#define SCATHA_CODEGEN_LABEL_H_

#include <string_view>

#include <utl/bit.hpp>
#include <utl/hash.hpp>

#include "Basic/Basic.h"

namespace scatha::codegen {
	
	using LabelType = i32; // Must be 32 bit because jump offsets are 32 bits.
						   // This way we are able to simply replace the labels by jump
						   // offsets while assembling without having to shift the code.
	
	inline LabelType makeLabel(LabelType value) { return value; }
	inline LabelType makeLabel(std::string_view name) { return utl::hash_string(name); }
	
	struct Label {
		explicit Label(LabelType value): value(value) {}
		explicit Label(std::string_view name): value(makeLabel(name)) {}
		LabelType value;
	};
	
	namespace internal {
		template <typename T>
		auto convertValue(T x) {
			if constexpr (std::floating_point<T>) {
				return utl::bit_cast<u64>(static_cast<f64>(x));
			}
			else {
				return static_cast<u64>(x);
			}
		}
		template <typename T>
		concept Arithmetic = std::integral<T> || std::floating_point<T>;
	}
	
	struct RR {
		explicit RR(u8 a, u8 b): a(a), b(b) {}
		u8 a, b;
	};
	struct RV {
		explicit RV(u8 r, internal::Arithmetic auto v): r(r), v(internal::convertValue(v)) {}
		u8 r;
		u64 v;
	};
	struct RM {
		explicit RM(u8 r, u8 ptrRegIdx, u8 offset, u8 offsetShift):
			r(r),
			ptrRegIdx(ptrRegIdx),
			offset(offset),
			offsetShift(offsetShift)
		{}
		u8 r;
		u8 ptrRegIdx;
		u8 offset;
		u8 offsetShift;
	};
	struct MR {
		explicit MR(u8 ptrRegIdx, u8 offset, u8 offsetShift, u8 r):
			ptrRegIdx(ptrRegIdx),
			offset(offset),
			offsetShift(offsetShift),
			r(r)
		{}
		u8 ptrRegIdx;
		u8 offset;
		u8 offsetShift;
		u8 r;
	};
	
	enum class Marker: u8 {
		Label = 0x80,
		OpCode = 0xFF,
	};
	
}

#endif // SCATHA_CODEGEN_LABEL_H_

