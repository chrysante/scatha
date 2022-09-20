#ifndef SCATHA_IC_TAC_H_
#define SCATHA_IC_TAC_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Sema/SymbolTable.h"

namespace scatha::ic {
	
	enum class Operation: u16 {
		mov, load,
		
		add, sub, mul, div, rem,
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
	
	struct TAS {
		struct Element {
			bool isVariable;
			bool isTemporary;
			u64 value;
		};
	
		static TAS::Element makeVariable(u64);
		static TAS::Element makeTemporary(u64);
		static TAS::Element makeValue(u64);
		
		Element getResult() const;
		Element getA() const;
		Element getB() const;
		
		bool isLabel      : 1;
		bool resultIsTemp : 1;
		bool aIsVar       : 1;
		bool aIsTemp      : 1;
		bool bIsVar       : 1;
		bool bIsTemp      : 1;
		union {
			Operation op;
			u16 label;
		};
		u64 result;
		u64 a;
		u64 b;
	};
	
	
	
	struct TAC {
		utl::vector<TAS> statements;
	};
	
}

#endif // SCATHA_IC_TAC_H_

