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
			
			{ param, "param" },
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
			{ sl,   "sl" },
			{ sr,   "sr" },
			{ And,  "and" },
			{ Or,   "or" },
			{ XOr,  "xor" },
			
			{ eq,   "eq" },
			{ neq,  "neq" },
			{ ils,  "ils" },
			{ ileq, "ileq" },
			{ ig,   "ig" },
			{ igeq, "igeq" },
			{ uls,  "uls" },
			{ uleq, "uleq" },
			{ ug,   "ug" },
			{ ugeq, "ugeq" },
			{ feq,  "feq" },
			{ fneq, "fneq" },
			{ fls,  "fls" },
			{ fleq, "fleq" },
			{ fg,   "fg" },
			{ fgeq, "fgeq" },
			
			{ lnt, "lnt" },
			{ bnt, "bnt" },
			
			{ jmp, "jmp" },
			
			{ ifPlaceholder, "ifPlaceholder(this should not be printed)" }
		});
	}
	
	std::ostream& operator<<(std::ostream& str, Operation op) {
		return str << toString(op);
	}
	
	int argumentCount(Operation op) {
		using enum Operation;
		return UTL_MAP_ENUM(op, int, {
			{ mov, 1 },
			
			{ param, 1 },
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
			{ sl,   2 },
			{ sr,   2 },
			{ And,  2 },
			{ Or,   2 },
			{ XOr,  2 },
			
			{ eq,   2 },
			{ neq,  2 },
			{ ils,  2 },
			{ ileq, 2 },
			{ ig,   2 },
			{ igeq, 2 },
			{ uls,  2 },
			{ uleq, 2 },
			{ ug,   2 },
			{ ugeq, 2 },
			
			{ feq,  2 },
			{ fneq, 2 },
			{ fls,  2 },
			{ fleq, 2 },
			{ fg,   2 },
			{ fgeq, 2 },
			
			{ lnt, 1 },
			{ bnt, 1 },
			
			{ jmp, 1 },
			
			{ ifPlaceholder, 1 }
		});
	}
	
	bool isJump(Operation op) {
		using enum Operation;
		return op == call || op == jmp;
	}

	bool isRelop(Operation op) {
		using enum Operation;
		return (utl::to_underlying(op) >= utl::to_underlying(eq) && utl::to_underlying(op) <= utl::to_underlying(fgeq));
	}
	
	Operation reverseRelop(Operation op) {
		using enum Operation;
		switch (op) {
			case eq: return neq;
			case neq: return eq;
			case ils: return igeq;
			case ileq: return ig;
			case uls: return ugeq;
			case uleq: return ug;
			case feq: return fneq;
			case fneq: return feq;
			case fls: return fgeq;
			case fleq: return fg;
			SC_NO_DEFAULT_CASE();
		}
	}
	
}
