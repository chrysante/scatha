#include "IC/TAS.h"

#include <ostream>
#include <iostream>

#include <utl/utility.hpp>

namespace scatha::ic {
	
	std::string_view toString(Operation op) {
		using enum Operation;
		return UTL_SERIALIZE_ENUM(op, {
			{ mov,  "mov" },
			{ load, "load" },
			
			{ add,  "add" },
			{ sub,  "sub" },
			{ mul,  "mul" },
			{ div,  "div" },
			{ idiv, "idiv" },
			{ rem,  "rem" },
			{ irem, "irem" },
			
			{ fadd, "fadd" },
			{ fsub, "fsub" },
			{ fmul, "fmul" },
			{ fdiv, "fdiv" },
			
			{ eq,  "eq" },
			{ neq, "neq" },
			{ ls,  "ls" },
			{ leq, "leq" },
			
			{ feq,  "feq" },
			{ fneq, "fneq" },
			{ fls,  "fls" },
			{ fleq, "fleq" },
			
			{ lnt, "lnt" },
			{ bnt, "bnt" },
			
			{ jmp,  "jmp" },
			{ cjmp, "cjmp" },
		});
	}
	
	int argumentCount(Operation op) {
		using enum Operation;
		return UTL_MAP_ENUM(op, int, {
			{ mov, 1 },
			{ load, 1 },
			
			{ add,  2 },
			{ sub,  2 },
			{ mul,  2 },
			{ div,  2 },
			{ idiv, 2 },
			{ rem,  2 },
			{ irem, 2 },
			
			{ fadd, 2 },
			{ fsub, 2 },
			{ fmul, 2 },
			{ fdiv, 2 },
			
			{ eq,  2 },
			{ neq, 2 },
			{ ls,  2 },
			{ leq, 2 },
			
			{ feq,  2 },
			{ fneq, 2 },
			{ fls,  2 },
			{ fleq, 2 },
			
			{ lnt, 1 },
			{ bnt, 1 },
			
			{ jmp, 1 },
			{ cjmp, 2 },
		});
	}
	
	TASElement TAS::makeVariable(u64 x, TASElement::Type type) {
		TASElement result;
		result.kind = TASElement::Variable;
		result.type = type;
		result.value = x;
		return result;
	}
	
	TASElement TAS::makeTemporary(u64 x, TASElement::Type type) {
		TASElement result;
		result.kind = TASElement::Temporary;
		result.type = type;
		result.value = x;
		return result;
	}
	
	TASElement TAS::makeLiteralValue(u64 value, TASElement::Type type) {
		TASElement result;
		result.kind = TASElement::LiteralValue;
		result.type = type;
		result.value = value;
		return result;
	}
	
	TASElement TAS::getResult() const {
		TASElement e;
		e.kind = resultKind;
		e.type = resultType;
		e.value = result;
		return e;
	}
	TASElement TAS::getA() const {
		TASElement e;
		e.kind = aKind;
		e.type = aType;
		e.value = a;
		return e;
	}
	TASElement TAS::getB() const {
		TASElement e;
		e.kind = bKind;
		e.type = bType;
		e.value = b;
		return e;
	}
	
}
