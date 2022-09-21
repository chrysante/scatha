#ifndef SCATHA_VM_LABEL_H_
#define SCATHA_VM_LABEL_H_

#include <string_view>

#include <utl/hash.hpp>

#include "Basic/Basic.h"

namespace scatha::vm {
	
	using LabelType = u32; // must be 32 bit
	
	LabelType makeLabel(LabelType value) { return value; }
	LabelType makeLabel(std::string_view name) { return utl::hash_string(name); }
	
	struct Label {
		explicit Label(LabelType value): value(value) {}
		explicit Label(std::string_view name): value(makeLabel(name)) {}
		LabelType value;
	};
	
}

#endif // SCATHA_VM_LABEL_H_

