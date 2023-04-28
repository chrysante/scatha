#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <svm/Builtin.h>

#include "AST/Fwd.h"
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
    ExpressionAnalysisResult analyze(ast::ReferenceExpression&);
    ExpressionAnalysisResult analyze(ast::Conditional&);
    ExpressionAnalysisResult analyze(ast::FunctionCall&);
    ExpressionAnalysisResult analyze(ast::Subscript&);

    ExpressionAnalysisResult analyze(ast::AbstractSyntaxTree&) {
        SC_DEBUGFAIL();
    }

    bool verifyConversion(ast::Expression const& from,
                          QualType const* to) const;

    QualType const* binaryOpResult(ast::BinaryExpression const&) const;

    SymbolID findExplicitCast(QualType const* targetType,
                              std::span<QualType const* const> from);

    SymbolTable& sym;
    IssueHandler& iss;
    /// Will be set by MemberAccess when right hand side is an identifier and
    /// unset by Identifier
    bool performRestrictedNameLookup = false;
};

} // namespace

ExpressionAnalysisResult analyzeExpression(ast::Expression& expr,
                                           SymbolTable& sym,
                                           IssueHandler& iss) {
    Context ctx{ .sym = sym, .iss = iss };
    return ctx.dispatch(expr);
}

ExpressionAnalysisResult Context::dispatch(ast::Expression& expr) {
    return visit(expr, [this](auto&& e) { return this->analyze(e); });
}

ExpressionAnalysisResult Context::analyze(ast::IntegerLiteral& l) {
    auto* type = sym.qualInt();
    l.decorate(SymbolID::Invalid, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::BooleanLiteral& l) {
    auto* type = sym.qualBool();
    l.decorate(SymbolID::Invalid, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::FloatingPointLiteral& l) {
    auto* type = sym.qualFloat();
    l.decorate(SymbolID::Invalid, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::StringLiteral& l) {
    auto* type = sym.qualString();
    l.decorate(SymbolID::Invalid, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::UnaryPrefixExpression& u) {
    auto const opResult = dispatch(*u.operand);
    if (!opResult) {
        return ExpressionAnalysisResult::fail();
    }
    auto const* operandType = u.operand->type();
    auto submitIssue        = [&] {
        iss.push<BadOperandForUnaryExpression>(u, operandType);
    };
    switch (u.operation()) {
    case ast::UnaryPrefixOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Negation:
        if (operandType->base() != sym.Int() &&
            operandType->base() != sym.Float())
        {
            submitIssue();
            return ExpressionAnalysisResult::fail();
        }
        break;
    case ast::UnaryPrefixOperator::BitwiseNot:
        if (operandType->base() != sym.Int()) {
            submitIssue();
            return ExpressionAnalysisResult::fail();
        }
        break;
    case ast::UnaryPrefixOperator::LogicalNot:
        if (operandType->base() != sym.Bool()) {
            submitIssue();
            return ExpressionAnalysisResult::fail();
        }
        break;
    case ast::UnaryPrefixOperator::Increment:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Decrement:
        // TODO: Check for mutability
        if (operandType->base() != sym.Int()) {
            submitIssue();
            return ExpressionAnalysisResult::fail();
        }
        break;
    case ast::UnaryPrefixOperator::_count:
        SC_DEBUGFAIL();
    }
    u.decorate(SymbolID::Invalid,
               sym.qualify(u.operand->type()),
               ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(u.type());
}

ExpressionAnalysisResult Context::analyze(ast::BinaryExpression& b) {
    auto const lhsRes = dispatch(*b.lhs);
    auto const rhsRes = dispatch(*b.rhs);
    if (!lhsRes || !rhsRes) {
        return ExpressionAnalysisResult::fail();
    }
    auto* resultType = binaryOpResult(b);
    if (!resultType) {
        return ExpressionAnalysisResult::fail();
    }
    b.decorate(SymbolID::Invalid, resultType, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(b.type());
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
            return sym.lookup(id.value());
        }
    }();
    if (!symbolID) {
        iss.push<UseOfUndeclaredIdentifier>(id, sym.currentScope());
        return ExpressionAnalysisResult::fail();
    }
    switch (symbolID.category()) {
    case SymbolCategory::Variable: {
        auto const& var = sym.get<Variable>(symbolID);
        id.decorate(symbolID, var.type(), ast::ValueCategory::LValue);
        return ExpressionAnalysisResult::lvalue(symbolID, var.type());
    }
    case SymbolCategory::Type: {
        id.decorate(symbolID,
                    nullptr,
                    ast::ValueCategory::None,
                    ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(
            sym.qualify(&sym.get<Type>(symbolID)));
    }
    case SymbolCategory::OverloadSet: {
        id.decorate(symbolID, nullptr, ast::ValueCategory::None);
        return ExpressionAnalysisResult::lvalue(symbolID, nullptr);
    }
    default:
        SC_DEBUGFAIL(); // Maybe push an issue here?
    }
}

ExpressionAnalysisResult Context::analyze(ast::MemberAccess& ma) {
    auto const objRes = dispatch(*ma.object);
    if (!objRes.success()) {
        return ExpressionAnalysisResult::fail();
    }
    Scope* lookupTargetScope = const_cast<ObjectType*>(objRes.type()->base());
    if (!lookupTargetScope) {
        return ExpressionAnalysisResult::fail();
    }
    auto* const oldScope = &sym.currentScope();
    sym.makeScopeCurrent(lookupTargetScope);
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(oldScope); };
    if (!isa<ast::Identifier>(*ma.member)) {
        iss.push<BadMemberAccess>(ma);
        return ExpressionAnalysisResult::fail();
    }
    /// We restrict name lookup to the
    /// current scope. This flag will be unset by the identifier case.
    performRestrictedNameLookup = true;
    auto const memRes           = dispatch(*ma.member);
    popScope.execute();
    if (!memRes.success()) {
        return ExpressionAnalysisResult::fail();
    }
    if (objRes.category() == ast::EntityCategory::Value &&
        memRes.category() != ast::EntityCategory::Value)
    {
        SC_DEBUGFAIL(); /// Can't look in a value and then in a type. probably
                        /// just return failure here
        return ExpressionAnalysisResult::fail();
    }
    /// Right hand side of member access expressions must be identifiers?
    auto const& memberIdentifier = cast<ast::Identifier&>(*ma.member);
    ma.decorate(memberIdentifier.symbolID(),
                memberIdentifier.type(),
                ma.object->valueCategory(),
                memRes.category());
    if (memRes.category() == ast::EntityCategory::Value) {
        SC_ASSERT(ma.type() == memRes.type(), "");
    }
    return memRes;
}

ExpressionAnalysisResult Context::analyze(ast::ReferenceExpression& ref) {
    auto const referredRes = dispatch(*ref.referred);
    if (!referredRes.success()) {
        return ExpressionAnalysisResult::fail();
    }
    auto const& referred = *ref.referred;
    if (referred.entityCategory() == ast::EntityCategory::Value) {
        if (referred.valueCategory() != ast::ValueCategory::LValue &&
            !referred.type()->isReference())
        {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return ExpressionAnalysisResult::fail();
        }
        auto* refType =
            sym.qualify(referred.type(), TypeQualifiers::ExplicitReference);
        ref.decorate(referred.symbolID(),
                     refType,
                     ast::ValueCategory::LValue,
                     ast::EntityCategory::Value);
        return ExpressionAnalysisResult::lvalue(referred.symbolID(), refType);
    }
    else {
        auto* type = sym.qualify(&sym.get<ObjectType>(referred.symbolID()));
        auto* refType =
            sym.addQualifiers(type, TypeQualifiers::ImplicitReference);
        ref.decorate(refType->symbolID(),
                     nullptr,
                     ast::ValueCategory::None,
                     ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(refType);
    }
}

ExpressionAnalysisResult Context::analyze(ast::Conditional& c) {
    dispatch(*c.condition);
    if (iss.fatal()) {
        return ExpressionAnalysisResult::fail();
    }
    verifyConversion(*c.condition, sym.qualify(sym.Bool()));
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
        iss.push<BadSymbolReference>(*c.ifExpr,
                                     ifRes.category(),
                                     ast::EntityCategory::Value);
        return ExpressionAnalysisResult::fail();
    }
    if (elseRes.category() != ast::EntityCategory::Value) {
        iss.push<BadSymbolReference>(*c.elseExpr,
                                     elseRes.category(),
                                     ast::EntityCategory::Value);
        return ExpressionAnalysisResult::fail();
    }
    if (ifRes.type() != elseRes.type()) {
        iss.push<BadOperandsForBinaryExpression>(c,
                                                 ifRes.type(),
                                                 elseRes.type());
        return ExpressionAnalysisResult::fail();
    }
    /// Maybe make this a global function
    auto combine = [](ast::ValueCategory a, ast::ValueCategory b) {
        return a == b ? a : ast::ValueCategory::RValue;
    };
    c.decorate(SymbolID::Invalid,
               ifRes.type(),
               combine(c.ifExpr->valueCategory(), c.elseExpr->valueCategory()));
    return ExpressionAnalysisResult::rvalue(ifRes.type());
}

ExpressionAnalysisResult Context::analyze(ast::Subscript&) { SC_DEBUGFAIL(); }

ExpressionAnalysisResult Context::analyze(ast::FunctionCall& fc) {
    bool success = true;
    utl::small_vector<QualType const*> argTypes;
    argTypes.reserve(fc.arguments.size());
    for (auto& arg: fc.arguments) {
        auto const argRes = dispatch(*arg);
        if (iss.fatal()) {
            return ExpressionAnalysisResult::fail();
        }
        success &= argRes.success();
        /// `arg` is undecorated if analysis of `arg` failed.
        argTypes.push_back(arg->isDecorated() ? arg->type() : nullptr);
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
    switch (objRes.symbolID().category()) {
    case SymbolCategory::OverloadSet: {
        auto const& overloadSet = sym.get<OverloadSet>(objRes.symbolID());
        auto const* functionPtr = overloadSet.find(argTypes);
        if (!functionPtr) {
            iss.push<BadFunctionCall>(
                fc,
                objRes.symbolID(),
                argTypes,
                BadFunctionCall::Reason::NoMatchingFunction);
            return ExpressionAnalysisResult::fail();
        }
        auto const& function = *functionPtr;
        fc.decorate(function.symbolID(),
                    /* typeID = */ function.signature().returnType(),
                    ast::ValueCategory::RValue);
        return ExpressionAnalysisResult::rvalue(fc.type());
    }

    case SymbolCategory::Type: {
        QualType const* targetType = objRes.type();
        SymbolID const castFn      = findExplicitCast(targetType, argTypes);
        if (!castFn) {
            // TODO: Make better error class here.
            iss.push<BadTypeConversion>(*fc.arguments.front(), targetType);
            return ExpressionAnalysisResult::fail();
        }
        fc.decorate(castFn, targetType, ast::ValueCategory::RValue);
        return ExpressionAnalysisResult::rvalue(fc.type());
    }

    default:
        iss.push<BadFunctionCall>(fc,
                                  SymbolID::Invalid,
                                  argTypes,
                                  BadFunctionCall::Reason::ObjectNotCallable);
        return ExpressionAnalysisResult::fail();
    }
}

bool Context::verifyConversion(ast::Expression const& expr,
                               QualType const* to) const {
    if (expr.type() != to) {
        iss.push<BadTypeConversion>(expr, to);
        return false;
    }
    return true;
}

QualType const* Context::binaryOpResult(
    ast::BinaryExpression const& expr) const {
    auto submitIssue = [&] {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs->type(),
                                                 expr.rhs->type());
    };
    auto verifySame = [&] {
        if (expr.lhs->type()->base() != expr.rhs->type()->base()) {
            submitIssue();
            return false;
        }
        return true;
    };
    auto verifyAnyOf =
        [&](Type const* toCheck, std::initializer_list<Type const*> types) {
        bool result = false;
        for (auto type: types) {
            if (toCheck == type) {
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
    case Multiplication:
        [[fallthrough]];
    case Division:
        [[fallthrough]];
    case Addition:
        [[fallthrough]];
    case Subtraction:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Int(), sym.Float() }))
        {
            return nullptr;
        }
        return expr.lhs->type();

    case Remainder:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Int() })) {
            return nullptr;
        }
        return sym.qualify(expr.lhs->type());

    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Int() })) {
            return nullptr;
        }
        return sym.qualify(expr.lhs->type());

    case LeftShift:
        [[fallthrough]];
    case RightShift:
        if (expr.lhs->type()->base() != sym.Int()) {
            submitIssue();
            return nullptr;
        }
        if (expr.rhs->type()->base() != sym.Int()) {
            submitIssue();
            return nullptr;
        }
        return sym.qualify(expr.lhs->type());

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Int(), sym.Float() }))
        {
            return nullptr;
        }
        return sym.qualBool();
    case Equals:
        [[fallthrough]];
    case NotEquals:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(),
                         { sym.Int(), sym.Float(), sym.Bool() }))
        {
            return nullptr;
        }
        return sym.qualBool();

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Bool() })) {
            return nullptr;
        }
        return sym.qualBool();

    case Assignment:
        if (!verifySame()) {
            return nullptr;
        }
        /// Assignments return `void` to prevent `if a = b { /* ... */ }` kind
        /// of errors
        return sym.qualVoid();
    case AddAssignment:
        [[fallthrough]];
    case SubAssignment:
        [[fallthrough]];
    case MulAssignment:
        [[fallthrough]];
    case DivAssignment:
        [[fallthrough]];
    case RemAssignment:
        [[fallthrough]];
    case LSAssignment:
        [[fallthrough]];
    case RSAssignment:
        [[fallthrough]];
    case AndAssignment:
        [[fallthrough]];
    case OrAssignment:
        [[fallthrough]];
    case XOrAssignment:
        if (!verifySame()) {
            return nullptr;
        }
        return sym.qualVoid();

    case Comma:
        return expr.rhs->type();

    case _count:
        SC_DEBUGFAIL();
    }
}

SymbolID Context::findExplicitCast(QualType const* to,
                                   std::span<QualType const* const> from) {
    if (from.size() != 1) {
        return SymbolID::Invalid;
    }
    if (from.front()->base() == sym.Int() && to->base() == sym.Float()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::i64tof64));
    }
    if (from.front()->base() == sym.Float() && to->base() == sym.Int()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::f64toi64));
    }
    return SymbolID::Invalid;
}

} // namespace scatha::sema
