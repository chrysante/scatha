#include "ExpressionAnalysis.h"

#include <optional>
#include <span>

#include "AST/Common.h"
#include "AST/Visit.h"

namespace scatha::sema {

	namespace {
		struct Context {
			ExpressionAnalysisResult dispatch(ast::Expression&);
			
			
			ExpressionAnalysisResult analyze(ast::IntegerLiteral&);
			ExpressionAnalysisResult analyze(ast::BooleanLiteral&);
			ExpressionAnalysisResult analyze(ast::FloatingPointLiteral&);
			ExpressionAnalysisResult analyze(ast::StringLiteral&);
			ExpressionAnalysisResult analyze(ast::UnaryPrefixExpression&);
			ExpressionAnalysisResult analyze(ast::BinaryExpression&);
			
			ExpressionAnalysisResult analyze(ast::Identifier&);
			ExpressionAnalysisResult analyze(ast::MemberAccess&);
			ExpressionAnalysisResult analyze(ast::Conditional&);
			ExpressionAnalysisResult analyze(ast::FunctionCall&);
			ExpressionAnalysisResult analyze(ast::Subscript&);
			
			ExpressionAnalysisResult analyze(ast::AbstractSyntaxTree&) { SC_DEBUGFAIL(); }
			
			bool verifyConversion(ast::Expression const& from, TypeID to) const;
			
			TypeID verifyBinaryOperation(ast::BinaryExpression const&) const;
			
			SymbolTable& sym;
			issue::IssueHandler* iss;
			/// Will be set by MemberAccess when right hand side is an identifier and unset by Identifier
			bool performRestrictedNameLookup = false;
		};
	}
	
	ExpressionAnalysisResult analyzeExpression(ast::Expression& expr, SymbolTable& sym, issue::IssueHandler* iss) {
		Context ctx{ .sym = sym, .iss = iss };
		return ctx.dispatch(expr);
	}
	
	ExpressionAnalysisResult Context::dispatch(ast::Expression& expr) {
		return ast::visit(expr, [this](auto&& e){ return analyze(e); });
	}
	
	ExpressionAnalysisResult Context::analyze(ast::IntegerLiteral& l) {
		l.typeID = sym.Int();
		return ExpressionAnalysisResult::rvalue(sym.Int());
	}
	
	ExpressionAnalysisResult Context::analyze(ast::BooleanLiteral& l) {
		l.typeID = sym.Bool();
		return ExpressionAnalysisResult::rvalue(sym.Bool());
	}
	
	ExpressionAnalysisResult Context::analyze(ast::FloatingPointLiteral& l) {
		l.typeID = sym.Float();
		return ExpressionAnalysisResult::rvalue(sym.Float());
	}
	
	ExpressionAnalysisResult Context::analyze(ast::StringLiteral& l) {
		l.typeID = sym.String();
		return ExpressionAnalysisResult::rvalue(sym.String());
	}
	
	ExpressionAnalysisResult Context::analyze(ast::UnaryPrefixExpression& u) {
		auto const opResult = dispatch(*u.operand);
		if (!opResult) {
			return ExpressionAnalysisResult::fail();
		}
		auto const& operandType = sym.getObjectType(u.operand->typeID);
		auto submitIssue = [&]{
			if (iss) {
				iss->push(BadOperandForUnaryExpression(u, operandType.symbolID()));
			}
		};
		if (!operandType.isBuiltin() || operandType.symbolID() == sym.String()) {
			submitIssue();
			return ExpressionAnalysisResult::fail();
		}
		switch (u.op) {
			case ast::UnaryPrefixOperator::Promotion: [[fallthrough]];
			case ast::UnaryPrefixOperator::Negation:
				if (operandType.symbolID() != sym.Int() &&
					operandType.symbolID() != sym.Float())
				{
					submitIssue();
					return ExpressionAnalysisResult::fail();
				}
				break;
			
			case ast::UnaryPrefixOperator::BitwiseNot:
				if (operandType.symbolID() != sym.Int()) {
					submitIssue();
					return ExpressionAnalysisResult::fail();
				}
				break;
			
			case ast::UnaryPrefixOperator::LogicalNot:
				if (operandType.symbolID() != sym.Bool()) {
					submitIssue();
					return ExpressionAnalysisResult::fail();
				}
				break;
			
			case ast::UnaryPrefixOperator::_count:
				SC_DEBUGFAIL();
		}
		u.typeID = u.operand->typeID;
		return ExpressionAnalysisResult::rvalue(u.typeID);
	}
	
	ExpressionAnalysisResult Context::analyze(ast::BinaryExpression& b) {
		auto const lhsRes = dispatch(*b.lhs);
		auto const rhsRes = dispatch(*b.rhs);
		if (!lhsRes || !rhsRes) {
			return ExpressionAnalysisResult::fail();
		}
		TypeID const resultType = verifyBinaryOperation(b);
		if (!resultType) {
			return ExpressionAnalysisResult::fail();
		}
		b.typeID = resultType;
		return ExpressionAnalysisResult::rvalue(b.typeID);
	}
	
	ExpressionAnalysisResult Context::analyze(ast::Identifier& id) {
		SymbolID const symbolID = [&]{
			if (performRestrictedNameLookup) {
				/// When we are on the right hand side of a member access expression we restrict lookup to the scope of the object of the left hand side.
				performRestrictedNameLookup = false;
				return sym.currentScope().findID(id.value());
			}
			else {
				return sym.lookup(id.token());
			}
		}();
		if (!symbolID) {
			if (iss) {
				iss->push(UseOfUndeclaredIdentifier(id, sym.currentScope()));
			}
			return ExpressionAnalysisResult::fail();
		}
		id.symbolID = symbolID;
		SymbolCategory const category = sym.categorize(symbolID);
		switch (category) {
			case SymbolCategory::Variable: {
				auto const& var = sym.getVariable(symbolID);
				id.typeID = var.typeID();
				return ExpressionAnalysisResult::lvalue(symbolID, var.typeID());
			}
			case SymbolCategory::ObjectType: {
				id.category = ast::EntityCategory::Type;
				return ExpressionAnalysisResult::type(symbolID);
			}
			case SymbolCategory::OverloadSet: {
				id.category = ast::EntityCategory::Value;
				return ExpressionAnalysisResult::lvalue(symbolID, TypeID::Invalid);
			}
			default:
				SC_DEBUGFAIL(); // Maybe throw something here?
		}
	}
	
	ExpressionAnalysisResult Context::analyze(ast::MemberAccess& ma) {
		auto const objRes = dispatch(*ma.object);
		if (!objRes.success()) {
			return ExpressionAnalysisResult::fail();
		}
		Scope* const lookupTargetScope = sym.tryGetObjectType(objRes.typeID());
		if (!lookupTargetScope) {
			return ExpressionAnalysisResult::fail();
		}
		auto* const oldScope = &sym.currentScope();
		sym.makeScopeCurrent(lookupTargetScope);
		utl::armed_scope_guard popScope = [&]{ sym.makeScopeCurrent(oldScope); };
		if (ma.member->nodeType() == ast::NodeType::Identifier) {
			/// When our member is an identifier we restrict name lookup to the current scope.
			/// This flag will be unset by the identifier case.
			performRestrictedNameLookup = true;
		}
		else {
			if (iss) iss->push(BadMemberAccess(ma));
			return ExpressionAnalysisResult::fail();
		}
		auto const memRes = dispatch(*ma.member);
		popScope.execute();
		if (!memRes.success()) {
			return ExpressionAnalysisResult::fail();
		}
		if (objRes.category() == ast::EntityCategory::Value &&
			memRes.category() != ast::EntityCategory::Value)
		{
			SC_DEBUGFAIL(); // can't look in a value an then in a type. probably just return failure here
			return ExpressionAnalysisResult::fail();
		}
		ma.category = memRes.category();
		// Right hand side of member access expressions must be identifiers?
		auto const& memberIdentifier = downCast<ast::Identifier>(*ma.member);
		ma.symbolID = memberIdentifier.symbolID;
		ma.typeID = memberIdentifier.typeID;
		if (memRes.category() == ast::EntityCategory::Value) {
			SC_ASSERT(ma.typeID == memRes.typeID(), "");
		}
		return memRes;
	}
	
	ExpressionAnalysisResult Context::analyze(ast::Conditional& c) {
		dispatch(*c.condition);
		if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
		verifyConversion(*c.condition, sym.Bool());
		if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
		auto const ifRes = dispatch(*c.ifExpr);
		if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
		auto const elseRes = dispatch(*c.elseExpr);
		if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
		if (!ifRes || !elseRes){
			return ExpressionAnalysisResult::fail();
		}
		if (ifRes.category() != ast::EntityCategory::Value) {
			if (iss) { iss->push(BadSymbolReference(*c.ifExpr, ifRes.category(), ast::EntityCategory::Value)); }
			return ExpressionAnalysisResult::fail();
		}
		if (elseRes.category() != ast::EntityCategory::Value) {
			if (iss) { iss->push(BadSymbolReference(*c.elseExpr, elseRes.category(), ast::EntityCategory::Value)); }
			return ExpressionAnalysisResult::fail();
		}
		if (ifRes.typeID() != elseRes.typeID()) {
			if (iss) { iss->push(BadOperandsForBinaryExpression(c, ifRes.typeID(), elseRes.typeID())); }
			return ExpressionAnalysisResult::fail();
		}
		c.typeID = ifRes.typeID();
		return ExpressionAnalysisResult::rvalue(ifRes.typeID());
	}
	
	ExpressionAnalysisResult Context::analyze(ast::Subscript&) {
		SC_DEBUGFAIL();
	}

	ExpressionAnalysisResult Context::analyze(ast::FunctionCall& fc) {
		bool success = true;
		utl::small_vector<TypeID> argTypes;
		argTypes.reserve(fc.arguments.size());
		for (auto& arg: fc.arguments) {
			auto const argRes = dispatch(*arg);
			if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
			success &= argRes.success();
			/// Invalid TypeID if analysis of arg failed.
			argTypes.push_back(arg->typeID);
		}
		auto const objRes = dispatch(*fc.object);
		if (iss && iss->fatal()) { return ExpressionAnalysisResult::fail(); }
		success &= objRes.success();
		if (!success) {
			return ExpressionAnalysisResult::fail();
		}
		if (!objRes) {
			return ExpressionAnalysisResult::fail();
		}
		/// We can only call lvalues right now which also must be overload sets (aka functions) until we have function pointers or overloading of operator()
		/// To implement the latter we must get the type of the expression and look in the scope for operator()
		/// It might be an idea to make all functions class types with defined operator()
		if (!objRes.isLValue()) {
			if (iss) {
				iss->push(BadFunctionCall(fc, SymbolID::Invalid, argTypes, BadFunctionCall::Reason::ObjectNotCallable));
			}
			return ExpressionAnalysisResult::fail();
		}
		if (!sym.is(objRes.symbolID(), SymbolCategory::OverloadSet)) {
			if (iss) {
				iss->push(BadFunctionCall(fc, SymbolID::Invalid, argTypes, BadFunctionCall::Reason::ObjectNotCallable));
			}
			return ExpressionAnalysisResult::fail();
		}
		auto const& overloadSet = sym.getOverloadSet(objRes.symbolID());
		
		auto const* functionPtr = overloadSet.find(argTypes);
		if (!functionPtr) {
			if (iss) {
				iss->push(BadFunctionCall(fc, objRes.symbolID(), argTypes, BadFunctionCall::Reason::NoMatchingFunction));
			}
			return ExpressionAnalysisResult::fail();
		}
		auto const& function = *functionPtr;
		fc.typeID = function.signature().returnTypeID();
		fc.functionID = function.symbolID();
		return ExpressionAnalysisResult::rvalue(fc.typeID);
	}
	
	bool Context::verifyConversion(ast::Expression const& from, TypeID to) const {
		if (from.typeID != to) {
			if (iss) { iss->push(BadTypeConversion(from, to)); }
			return false;
		}
		return true;
	}
	
	TypeID Context::verifyBinaryOperation(ast::BinaryExpression const& expr) const {
		auto submitIssue = [&]{
			if (iss) {
				iss->push(BadOperandsForBinaryExpression(expr, expr.lhs->typeID, expr.rhs->typeID));
			}
		};
		auto verifySame = [&]{
			if (expr.lhs->typeID != expr.rhs->typeID) {
				submitIssue();
				return false;
			}
			return true;
		};
		auto verifyAnyOf = [&](TypeID toCheck, std::initializer_list<TypeID> ids) {
			bool result = false;
			for (auto id: ids) {
				if (toCheck == id) { result = true; }
			}
			if (!result) {
				submitIssue();
				return false;
			}
			return true;
		};
		
		switch (expr.op) {
			using enum ast::BinaryOperator;
			case Multiplication: [[fallthrough]];
			case Division:       [[fallthrough]];
			case Addition:       [[fallthrough]];
			case Subtraction:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float() })) { return TypeID::Invalid; }
				return expr.lhs->typeID;
				
			case Remainder:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Int() })) { return TypeID::Invalid; }
				return expr.lhs->typeID;
				
			case BitwiseAnd:     [[fallthrough]];
			case BitwiseXOr:     [[fallthrough]];
			case BitwiseOr:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Int() })) { return TypeID::Invalid; }
				return expr.lhs->typeID;
				
			case LeftShift:      [[fallthrough]];
			case RightShift:
				if (expr.lhs->typeID != sym.Int()) {
					submitIssue();
					return TypeID::Invalid;
				}
				if (expr.rhs->typeID != sym.Int()) {
					submitIssue();
					return TypeID::Invalid;
				}
				return expr.lhs->typeID;
				
			case Less:           [[fallthrough]];
			case LessEq:         [[fallthrough]];
			case Greater:        [[fallthrough]];
			case GreaterEq:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float() })) { return TypeID::Invalid; }
				return sym.Bool();
			case Equals:         [[fallthrough]];
			case NotEquals:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Int(), sym.Float(), sym.Bool() })) { return TypeID::Invalid; }
				return sym.Bool();
				
				
			case LogicalAnd:     [[fallthrough]];
			case LogicalOr:
				if (!verifySame()) { return TypeID::Invalid; }
				if (!verifyAnyOf(expr.lhs->typeID, { sym.Bool() })) { return TypeID::Invalid; }
				return sym.Bool();
				
			case Assignment:     [[fallthrough]];
			case AddAssignment:  [[fallthrough]];
			case SubAssignment:  [[fallthrough]];
			case MulAssignment:  [[fallthrough]];
			case DivAssignment:  [[fallthrough]];
			case RemAssignment:  [[fallthrough]];
			case LSAssignment:   [[fallthrough]];
			case RSAssignment:   [[fallthrough]];
			case AndAssignment:  [[fallthrough]];
			case OrAssignment:   [[fallthrough]];
			case XOrAssignment:
				if (!verifySame()) { return TypeID::Invalid; }
				return sym.Void();
				
			case Comma:
				return expr.rhs->typeID;
				
			case _count:
				SC_DEBUGFAIL();
		}
	}
	
}
