#ifndef SCATHA_IC_TAC_H_
#define SCATHA_IC_TAC_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	enum class Operation: u8 {
		mov, load,
		
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
	
	struct TASElement {
		enum Kind: u8 {
			Variable, Temporary, LiteralValue
		};
		enum Type: u8 {
			Bool, Signed, Unsigned, Float
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
		
		TASElement getResult() const;
		TASElement getA() const;
		TASElement getB() const;
		
		bool isLabel;                // 1 byte
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
			u64 label;
		};
		u64 a;
		u64 b;
	};
	
	
	
	struct TAC {
		utl::vector<TAS> statements;
	};
	
}

#endif // SCATHA_IC_TAC_H_

