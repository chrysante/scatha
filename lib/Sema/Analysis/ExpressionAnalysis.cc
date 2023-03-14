#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include "AST/Common.h"

#include "Sema/SemanticIssue.h"

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

    ExpressionAnalysisResult analyze(ast::AbstractSyntaxTree&) {
        SC_DEBUGFAIL();
    }

    bool verifyConversion(ast::Expression const& from, TypeID to) const;

    TypeID verifyBinaryOperation(ast::BinaryExpression const&) const;

    SymbolTable& sym;
    issue::SemaIssueHandler& iss;
    /// Will be set by MemberAccess when right hand side is an identifier and
    /// unset by Identifier
    bool performRestrictedNameLookup = false;
};

} // namespace

ExpressionAnalysisResult analyzeExpression(ast::Expression& expr,
                                           SymbolTable& sym,
                                           issue::SemaIssueHandler& iss) {
    Context ctx{ .sym = sym, .iss = iss };
    return ctx.dispatch(expr);
}

ExpressionAnalysisResult Context::dispatch(ast::Expression& expr) {
    return visit(expr, [this](auto&& e) { return this->analyze(e); });
}

ExpressionAnalysisResult Context::analyze(ast::IntegerLiteral& l) {
    l.decorate(sym.Int(), ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(sym.Int());
}

ExpressionAnalysisResult Context::analyze(ast::BooleanLiteral& l) {
    l.decorate(sym.Bool(), ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(sym.Bool());
}

ExpressionAnalysisResult Context::analyze(ast::FloatingPointLiteral& l) {
    l.decorate(sym.Float(), ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(sym.Float());
}

ExpressionAnalysisResult Context::analyze(ast::StringLiteral& l) {
    l.decorate(sym.String(), ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(sym.String());
}

ExpressionAnalysisResult Context::analyze(ast::UnaryPrefixExpression& u) {
    auto const opResult = dispatch(*u.operand);
    if (!opResult) {
        return ExpressionAnalysisResult::fail();
    }
    auto const& operandType = sym.getObjectType(u.operand->typeID());
    auto submitIssue        = [&] {
        iss.push(BadOperandForUnaryExpression(u, operandType.symbolID()));
    };
    if (!operandType.isBuiltin() || operandType.symbolID() == sym.String()) {
        submitIssue();
        return ExpressionAnalysisResult::fail();
    }
    switch (u.operation()) {
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
    case ast::UnaryPrefixOperator::Increment: [[fallthrough]];
    case ast::UnaryPrefixOperator::Decrement:
        if (operandType.symbolID() != sym.Int()) {
            submitIssue();
            return ExpressionAnalysisResult::fail();
        }
        break;
    case ast::UnaryPrefixOperator::_count: SC_DEBUGFAIL();
    }
    u.decorate(u.operand->typeID(), ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(u.typeID());
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
    b.decorate(resultType, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(b.typeID());
}

ExpressionAnalysisResult Context::analyze(ast::Identifier& id) {
    SymbolID const symbolID = [&] {
        if (performRestrictedNameLookup) {
            /// When we are on the right hand side of a member access expression
            /// we restrict lookup to the scope of the object of the left hand
            /// side.
            performRestrictedNameLookup = false;
            return sym.currentScope().findID(id.value());
        }
        else {
            return sym.lookup(id.token());
        }
    }();
    if (!symbolID) {
        iss.push(UseOfUndeclaredIdentifier(id, sym.currentScope()));
        return ExpressionAnalysisResult::fail();
    }
    switch (symbolID.category()) {
    case SymbolCategory::Variable: {
        auto const& var = sym.getVariable(symbolID);
        id.decorate(symbolID, var.typeID(), ast::ValueCategory::LValue);
        return ExpressionAnalysisResult::lvalue(symbolID, var.typeID());
    }
    case SymbolCategory::ObjectType: {
        id.decorate(symbolID,
                    TypeID::Invalid,
                    ast::ValueCategory::None,
                    ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(symbolID);
    }
    case SymbolCategory::OverloadSet: {
        id.decorate(symbolID, TypeID::Invalid, ast::ValueCategory::None);
        return ExpressionAnalysisResult::lvalue(symbolID, TypeID::Invalid);
    }
    default: SC_DEBUGFAIL(); // Maybe push an issue here?
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
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(oldScope); };
    if (ma.member->nodeType() == ast::NodeType::Identifier) {
        /// When our member is an identifier we restrict name lookup to the
        /// current scope. This flag will be unset by the identifier case.
        performRestrictedNameLookup = true;
    }
    else {
        iss.push(BadMemberAccess(ma));
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
        SC_DEBUGFAIL(); // can't look in a value an then in a type. probably
                        // just return failure here
        return ExpressionAnalysisResult::fail();
    }
    // Right hand side of member access expressions must be identifiers?
    auto const& memberIdentifier = cast<ast::Identifier&>(*ma.member);
    ma.decorate(memberIdentifier.symbolID(),
                memberIdentifier.typeID(),
                ma.object->valueCategory(),
                memRes.category());
    if (memRes.category() == ast::EntityCategory::Value) {
        SC_ASSERT(ma.typeID() == memRes.typeID(), "");
    }
    return memRes;
}

ExpressionAnalysisResult Context::analyze(ast::Conditional& c) {
    dispatch(*c.condition);
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    verifyConversion(*c.condition, sym.Bool());
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    auto const ifRes = dispatch(*c.ifExpr);
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    auto const elseRes = dispatch(*c.elseExpr);
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    if (!ifRes || !elseRes) {
        return ExpressionAnalysisResult::fail();
    }
    if (ifRes.category() != ast::EntityCategory::Value) {
        iss.push(BadSymbolReference(*c.ifExpr,
                                    ifRes.category(),
                                    ast::EntityCategory::Value));
        return ExpressionAnalysisResult::fail();
    }
    if (elseRes.category() != ast::EntityCategory::Value) {
        iss.push(BadSymbolReference(*c.elseExpr,
                                    elseRes.category(),
                                    ast::EntityCategory::Value));
        return ExpressionAnalysisResult::fail();
    }
    if (ifRes.typeID() != elseRes.typeID()) {
        iss.push(BadOperandsForBinaryExpression(c,
                                                ifRes.typeID(),
                                                elseRes.typeID()));
        return ExpressionAnalysisResult::fail();
    }
    /// Maybe make this a global function
    auto combine = [](ast::ValueCategory a, ast::ValueCategory b) {
        return a == b ? a : ast::ValueCategory::RValue;
    };
    c.decorate(ifRes.typeID(),
               combine(c.ifExpr->valueCategory(), c.elseExpr->valueCategory()));
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
        if (iss.fatal()) {
            return ExpressionAnalysisResult::fail();
        }
        success &= argRes.success();
        /// `arg` is undecorated if analysis of `arg` failed.
        argTypes.push_back(arg->isDecorated() ? arg->typeID() :
                                                TypeID::Invalid);
    }
    auto const objRes = dispatch(*fc.object);
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    success &= objRes.success();
    if (!success) {
        return ExpressionAnalysisResult::fail();
    }
    if (!objRes) {
        return ExpressionAnalysisResult::fail();
    }
    /// We can only call lvalues right now which also must be overload sets (aka
    /// functions) until we have function pointers or overloading of operator().
    /// To implement the latter we must get the type of the expression and look
    /// in the scope for `operator()` It might be an idea to make all functions
    /// class types with defined `operator()`
    if (!objRes.isLValue()) {
        iss.push(BadFunctionCall(fc,
                                 SymbolID::Invalid,
                                 argTypes,
                                 BadFunctionCall::Reason::ObjectNotCallable));
        return ExpressionAnalysisResult::fail();
    }
    if (objRes.symbolID().category() != SymbolCategory::OverloadSet) {
        iss.push(BadFunctionCall(fc,
                                 SymbolID::Invalid,
                                 argTypes,
                                 BadFunctionCall::Reason::ObjectNotCallable));
        return ExpressionAnalysisResult::fail();
    }
    auto const& overloadSet = sym.getOverloadSet(objRes.symbolID());

    auto const* functionPtr = overloadSet.find(argTypes);
    if (!functionPtr) {
        iss.push(BadFunctionCall(fc,
                                 objRes.symbolID(),
                                 argTypes,
                                 BadFunctionCall::Reason::NoMatchingFunction));
        return ExpressionAnalysisResult::fail();
    }
    auto const& function = *functionPtr;
    fc.decorate(function.symbolID(),
                /* typeID = */ function.signature().returnTypeID(),
                ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(fc.typeID());
}

bool Context::verifyConversion(ast::Expression const& from, TypeID to) const {
    if (from.typeID() != to) {
        iss.push(BadTypeConversion(from, to));
        return false;
    }
    return true;
}

TypeID Context::verifyBinaryOperation(ast::BinaryExpression const& expr) const {
    auto submitIssue = [&] {
        iss.push(BadOperandsForBinaryExpression(expr,
                                                expr.lhs->typeID(),
                                                expr.rhs->typeID()));
    };
    auto verifySame = [&] {
        if (expr.lhs->typeID() != expr.rhs->typeID()) {
            submitIssue();
            return false;
        }
        return true;
    };
    auto verifyAnyOf = [&](TypeID toCheck, std::initializer_list<TypeID> ids) {
        bool result = false;
        for (auto id: ids) {
            if (toCheck == id) {
                result = true;
            }
        }
        if (!result) {
            submitIssue();
            return false;
        }
        return true;
    };

    switch (expr.operation()) {
        using enum ast::BinaryOperator;
    case Multiplication: [[fallthrough]];
    case Division: [[fallthrough]];
    case Addition: [[fallthrough]];
    case Subtraction:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(), { sym.Int(), sym.Float() })) {
            return TypeID::Invalid;
        }
        return expr.lhs->typeID();

    case Remainder:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(), { sym.Int() })) {
            return TypeID::Invalid;
        }
        return expr.lhs->typeID();

    case BitwiseAnd: [[fallthrough]];
    case BitwiseXOr: [[fallthrough]];
    case BitwiseOr:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(), { sym.Int() })) {
            return TypeID::Invalid;
        }
        return expr.lhs->typeID();

    case LeftShift: [[fallthrough]];
    case RightShift:
        if (expr.lhs->typeID() != sym.Int()) {
            submitIssue();
            return TypeID::Invalid;
        }
        if (expr.rhs->typeID() != sym.Int()) {
            submitIssue();
            return TypeID::Invalid;
        }
        return expr.lhs->typeID();

    case Less: [[fallthrough]];
    case LessEq: [[fallthrough]];
    case Greater: [[fallthrough]];
    case GreaterEq:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(), { sym.Int(), sym.Float() })) {
            return TypeID::Invalid;
        }
        return sym.Bool();
    case Equals: [[fallthrough]];
    case NotEquals:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(),
                         { sym.Int(), sym.Float(), sym.Bool() }))
        {
            return TypeID::Invalid;
        }
        return sym.Bool();

    case LogicalAnd: [[fallthrough]];
    case LogicalOr:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        if (!verifyAnyOf(expr.lhs->typeID(), { sym.Bool() })) {
            return TypeID::Invalid;
        }
        return sym.Bool();

    case Assignment: [[fallthrough]];
    case AddAssignment: [[fallthrough]];
    case SubAssignment: [[fallthrough]];
    case MulAssignment: [[fallthrough]];
    case DivAssignment: [[fallthrough]];
    case RemAssignment: [[fallthrough]];
    case LSAssignment: [[fallthrough]];
    case RSAssignment: [[fallthrough]];
    case AndAssignment: [[fallthrough]];
    case OrAssignment: [[fallthrough]];
    case XOrAssignment:
        if (!verifySame()) {
            return TypeID::Invalid;
        }
        return sym.Void();

    case Comma: return expr.rhs->typeID();

    case _count: SC_DEBUGFAIL();
    }
}

} // namespace scatha::sema
