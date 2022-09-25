#include "IC/ThreeAddressStatement.h"

#include <ostream>
#include <string_view>

#include <utl/utility.hpp>

namespace scatha::ic {
	
	FunctionLabel::FunctionLabel(ast::FunctionDefinition const& def):
		_functionID(def.symbolID)
	{
		_parameters.reserve(def.parameters.size());
		for (auto& p: def.parameters) {
			_parameters.push_back({ p->symbolID, p->typeID });
		}
	}
	
	std::string_view toString(Operation op) {
		using enum Operation;
		return UTL_SERIALIZE_ENUM(op, {
			{ mov,  "mov" },
			
			{ pushParam, "pushParam" },
			{ getResult, "getResult" },
			{ call,      "call" },
			{ ret,       "ret" },
			
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
			
			{ eq,   "eq" },
			{ neq,  "neq" },
			{ ils,  "ils" },
			{ ileq, "ileq" },
			{ uls,  "uls" },
			{ uleq, "uleq" },
			
			{ feq,  "feq" },
			{ fneq, "fneq" },
			{ fls,  "fls" },
			{ fleq, "fleq" },
			
			{ lnt, "lnt" },
			{ bnt, "bnt" },
			
			{ jmp, "jmp" },
			{ je,  "je" },
			{ jne, "jne" },
			{ jl,  "jl" },
			{ jle, "jle" },
			{ jg,  "jg" },
			{ jge, "jge" },
		});
	}
	
	std::ostream& operator<<(std::ostream& str, Operation op) {
		return str << toString(op);
	}
	
	int argumentCount(Operation op) {
		using enum Operation;
		return UTL_MAP_ENUM(op, int, {
			{ mov, 1 },
			
			{ pushParam, 1 },
			{ getResult, 0 },
			{ call, 1 },
			{ ret, 1 },
			
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
			
			{ eq,   2 },
			{ neq,  2 },
			{ ils,  2 },
			{ ileq, 2 },
			{ uls,  2 },
			{ uleq, 2 },
			
			{ feq,  2 },
			{ fneq, 2 },
			{ fls,  2 },
			{ fleq, 2 },
			
			{ lnt, 1 },
			{ bnt, 1 },
			
			{ jmp, 1 },
			{ je,  1 },
			{ jne, 1 },
			{ jl,  1 },
			{ jle, 1 },
			{ jg,  1 },
			{ jge, 1 },
		});
	}
	
	bool isJump(Operation op) {
		using enum Operation;
		return op == call || (utl::to_underlying(op) >= utl::to_underlying(jmp) && utl::to_underlying(op) <= utl::to_underlying(jge));
	}
	
}
