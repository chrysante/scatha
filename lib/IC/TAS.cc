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
			
			{ add, "add" },
			{ sub, "sub" },
			{ mul, "mul" },
			{ div, "div" },
			{ rem, "rem" },
			
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
			
			{ add, 2 },
			{ sub, 2 },
			{ mul, 2 },
			{ div, 2 },
			{ rem, 2 },
			
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
	
	TAS::Element TAS::makeVariable(u64 id) {
		TAS::Element result;
		result.isVariable = true;
		result.isTemporary = false;
		result.value = id;
		return result;
	}
	
	TAS::Element TAS::makeTemporary(u64 id) {
		TAS::Element result;
		result.isVariable = true;
		result.isTemporary = true;
		result.value = id;
		return result;
	}
	
	TAS::Element TAS::makeValue(u64 value) {
		TAS::Element result;
		result.isVariable = false;
		result.isTemporary = false;
		result.value = value;
		return result;
	}
	
	TAS::Element TAS::getResult() const {
		Element e;
		e.isVariable = true;
		e.isTemporary = resultIsTemp;
		e.value = result;
		return e;
	}
	TAS::Element TAS::getA() const {
		Element e;
		e.isVariable = aIsVar;
		e.isTemporary = aIsTemp;
		e.value = a;
		return e;
	}
	TAS::Element TAS::getB() const {
		Element e;
		e.isVariable = bIsVar;
		e.isTemporary = bIsTemp;
		e.value = b;
		return e;
	}
	
}
