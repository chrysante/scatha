#ifndef SCATHA_IC_TAC_H_
#define SCATHA_IC_TAC_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	enum class Operation: u8 {
		mov,
		
		pushParam,
		getResult,
		call,
		ret,
		
		add, sub, mul, div, idiv, rem, irem,
		fadd, fsub, fmul, fdiv,
		
		eq, neq, ls, leq,
		feq, fneq, fls, fleq,
		
		lnt, // logical not
		bnt, // bitwise not
		
		jmp, cjmp,
		
		_count
	};

	std::string_view toString(Operation);
	
	int argumentCount(Operation);
	
	inline bool isJump(Operation op) { return op == Operation::jmp || op == Operation::cjmp || op == Operation::call; }
	
	struct TASElement {
		enum Kind: u8 {
			Variable, Temporary, LiteralValue, Label
		};
		
		enum Type: u8 {
			Void, Bool, Signed, Unsigned, Float
		};
		
		Kind kind;
		Type type;
		u64 value;
	};
	
	/**
	 * Three address statement
	 * Encodes
	 */
	struct TAS {
		static TASElement makeVariable(u64, TASElement::Type);
		static TASElement makeTemporary(u64, TASElement::Type);
		static TASElement makeLiteralValue(u64, TASElement::Type);
		static TASElement makeLabel(u64);
		
		TASElement getResult() const;
		TASElement getA() const;
		TASElement getB() const;
		
		bool isLabel = false;        // 1 byte
		TASElement::Kind resultKind; // 1 byte
		TASElement::Type resultType; // 1 byte
		TASElement::Kind aKind;      // 1 byte
		TASElement::Type aType;      // 1 byte
		TASElement::Kind bKind;      // 1 byte
		TASElement::Type bType;      // 1 byte
		
		Operation op;                // 1 byte
									 // ----------------
									 // 8 bytes in total
		union {
			u64 result;
			u64 functionID;
		};
		union {
			u64 a;
			u64 labelIndex;
		};
		u64 b;
	};
	
	
	
	struct TAC {
		utl::vector<TAS> statements;
	};
	
}

#endif // SCATHA_IC_TAC_H_

