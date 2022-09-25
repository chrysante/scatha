#ifndef SCATHA_IC_THREEADDRESSSTATEMENT_H_
#define SCATHA_IC_THREEADDRESSSTATEMENT_H_

#include <iosfwd>
#include <variant>

#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "AST/Expression.h"
#include "Sema/SemanticElements.h"

namespace scatha::ic {
	
	struct EmptyArgument {
		sema::TypeID typeID;
	};
	
	struct Variable {
		explicit Variable(sema::SymbolID id):
			_id(id)
		{}

		sema::SymbolID id() const { return _id; }

	private:
		sema::SymbolID _id;
	};
	
	struct Temporary {
		size_t index;
		sema::TypeID type;
	};
	
	struct LiteralValue {
		LiteralValue(u64 value, sema::TypeID type):
			value(value),
			type(type)
		{}
		
		explicit LiteralValue(ast::IntegerLiteral const& lit):
			LiteralValue(lit.value, lit.typeID)
		{}
		
		explicit LiteralValue(ast::FloatingPointLiteral const& lit):
			LiteralValue(utl::bit_cast<u64>(lit.value), lit.typeID)
		{}
		
		u64 value;
		sema::TypeID type;
	};
	
	struct Label {
		Label() = default;
		explicit Label(sema::SymbolID functionID, i64 index = -1):
			functionID(functionID),
			index(index)
		{}
		
		sema::SymbolID functionID;
		i64 index = -1;
	};
	
	struct FunctionLabel {
		struct Parameter {
			sema::SymbolID id;
			sema::TypeID type;
		};
		
	public:
		explicit FunctionLabel(ast::FunctionDefinition const&);
		
		sema::SymbolID functionID() const { return _functionID; };
	
		std::span<Parameter const> parameters() const { return _parameters; }
		
	private:
		utl::small_vector<Parameter> _parameters;
		sema::SymbolID _functionID;
	};
	
	using TasArgumentTypeVariant = std::variant<EmptyArgument, Variable, Temporary, LiteralValue, Label>;
	
	struct TasArgument: TasArgumentTypeVariant {
		using TasArgumentTypeVariant::TasArgumentTypeVariant;
		TasArgument(): TasArgumentTypeVariant(EmptyArgument{}) {}
		
		template <typename... F>
		decltype(auto) visit(utl::visitor<F...> const& visitor) {
			return std::visit(visitor, *this);
		}
		
		template <typename... F>
		decltype(auto) visit(utl::visitor<F...> const& visitor) const {
			return std::visit(visitor, *this);
		}
		
		enum Kind {
			empty, variable, temporary, literalValue, label
		};
		
		bool is(Kind kind) const { return index() == kind; }
		
		template <typename T>
		T& as() { return std::get<T>(*this); }
		template <typename T>
		T const& as() const { return std::get<T>(*this); }
	};
	
	enum class Operation: u8 {
		mov,        // result <- arg
		
		pushParam,  // void   <- arg
		
		getResult,  // result <- void
		call,       // void   <- Label
		ret,        // void   <- void
		
		add,        // result <- arg1, arg2
		sub,        // result <- arg1, arg2
		mul,        // result <- arg1, arg2
		div,        // result <- arg1, arg2
		idiv,       // result <- arg1, arg2
		rem,        // result <- arg1, arg2
		irem,       // result <- arg1, arg2
		fadd,       // result <- arg1, arg2
		fsub,       // result <- arg1, arg2
		fmul,       // result <- arg1, arg2
		fdiv,       // result <- arg1, arg2
		
		eq, neq, ls, leq,
		feq, fneq, fls, fleq,
		
		lnt, // logical not
		bnt, // bitwise not
		
		jmp, cjmp,
		
		_count
	};
	
	std::string_view toString(Operation);
	
	std::ostream& operator<<(std::ostream&, Operation);
	
	int argumentCount(Operation);
	
	bool isJump(Operation);
	
	struct ThreeAddressStatement {
		Operation operation;
		TasArgument result;
		TasArgument arg1;
		TasArgument arg2;
	};
	
}

#endif // SCATHA_IC_THREEADDRESSSTATEMENT_H_

