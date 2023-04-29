#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/Fwd.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

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
    ExpressionAnalysisResult analyze(ast::UniqueExpression&);
    ExpressionAnalysisResult analyze(ast::Conditional&);
    ExpressionAnalysisResult analyze(ast::FunctionCall&);
    ExpressionAnalysisResult analyze(ast::Subscript&);
    ExpressionAnalysisResult analyze(ast::ListExpression&);

    ExpressionAnalysisResult analyze(ast::AbstractSyntaxTree&) {
        SC_DEBUGFAIL();
    }

    bool expectValue(ast::Expression const& expr);

    bool verifyConversion(ast::Expression const& from,
                          QualType const* to) const;

    QualType const* binaryOpResult(ast::BinaryExpression const&) const;

    Function* findExplicitCast(QualType const* targetType,
                               std::span<QualType const* const> from);

    QualType const* stripQualifiers(QualType const* type) const {
        return sym.qualify(type->base());
    }

    SymbolTable& sym;
    IssueHandler& iss;
    /// Will be set by MemberAccess when right hand side is an identifier and
    /// unset by Identifier
    bool performRestrictedNameLookup = false;
};

} // namespace

ExpressionAnalysisResult sema::analyzeExpression(ast::Expression& expr,
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
    l.decorate(nullptr, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::BooleanLiteral& l) {
    auto* type = sym.qualBool();
    l.decorate(nullptr, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::FloatingPointLiteral& l) {
    auto* type = sym.qualFloat();
    l.decorate(nullptr, type, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyze(ast::StringLiteral& l) {
    auto* type = sym.qualString();
    l.decorate(nullptr, type, ast::ValueCategory::RValue);
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
    u.decorate(nullptr,
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
    b.decorate(nullptr, resultType, ast::ValueCategory::RValue);
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
        auto& var = sym.get<Variable>(symbolID);
        id.decorate(&var, var.type(), ast::ValueCategory::LValue);
        return ExpressionAnalysisResult::lvalue(&var, var.type());
    }
    case SymbolCategory::Type: {
        auto& type = sym.get<Type>(symbolID);
        id.decorate(&type,
                    nullptr,
                    ast::ValueCategory::None,
                    ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(sym.qualify(&type));
    }
    case SymbolCategory::OverloadSet: {
        auto& os = sym.get<OverloadSet>(symbolID);
        id.decorate(&os, nullptr, ast::ValueCategory::None);
        return ExpressionAnalysisResult::lvalue(&os, nullptr);
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
    Scope* lookupTargetScope = [&] {
        if (objRes.category() == ast::EntityCategory::Type) {
            auto* type = cast<QualType*>(objRes.entity())->base();
            return const_cast<ObjectType*>(type);
        }
        else {
            return const_cast<ObjectType*>(objRes.type()->base());
        }
    }();
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
    auto& memberIdentifier = cast<ast::Identifier&>(*ma.member);
    ma.decorate(memberIdentifier.entity(),
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
    auto& referred = *ref.referred;
    if (referred.entityCategory() == ast::EntityCategory::Value) {
        if (referred.valueCategory() != ast::ValueCategory::LValue &&
            !referred.type()->isReference())
        {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return ExpressionAnalysisResult::fail();
        }
        auto* refType =
            sym.qualify(referred.type(), TypeQualifiers::ExplicitReference);
        ref.decorate(referred.entity(),
                     refType,
                     ast::ValueCategory::LValue,
                     ast::EntityCategory::Value);
        return ExpressionAnalysisResult::lvalue(referred.entity(), refType);
    }
    else {
        auto* type = sym.qualify(cast<Type const*>(referred.entity()));
        auto* refType =
            sym.addQualifiers(type, TypeQualifiers::ImplicitReference);
        ref.decorate(const_cast<QualType*>(refType),
                     nullptr,
                     ast::ValueCategory::None,
                     ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(refType);
    }
}

ExpressionAnalysisResult Context::analyze(ast::UniqueExpression& expr) {
    auto* initExpr = dyncast<ast::FunctionCall*>(expr.initExpr.get());
    if (!initExpr) {
        iss.push<BadExpression>(*expr.initExpr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    auto* typeExpr   = initExpr->object.get();
    auto typeExprRes = dispatch(*typeExpr);
    if (!typeExprRes || typeExprRes.category() != ast::EntityCategory::Type) {
        iss.push<BadExpression>(*typeExpr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    SC_ASSERT(initExpr->arguments.size() == 1, "Implement this properly");
    auto* argument = initExpr->arguments.front().get();
    auto argRes    = dispatch(*argument);
    if (!argRes || argRes.category() != ast::EntityCategory::Value) {
        iss.push<BadExpression>(*argument, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    auto* rawType = cast<Type const*>(typeExpr->entity());
    auto* type =
        sym.qualify(rawType,
                    TypeQualifiers::ExplicitReference | TypeQualifiers::Unique);
    expr.decorate(type);
    return ExpressionAnalysisResult::rvalue(type);
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
    c.decorate(nullptr,
               ifRes.type(),
               combine(c.ifExpr->valueCategory(), c.elseExpr->valueCategory()));
    return ExpressionAnalysisResult::rvalue(ifRes.type());
}

ExpressionAnalysisResult Context::analyze(ast::Subscript& expr) {
    dispatch(*expr.object);
    if (!expectValue(*expr.object)) {
        return ExpressionAnalysisResult::fail();
    }
    if (!expr.object->type()->isArray()) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    for (auto& arg: expr.arguments) {
        dispatch(*arg);
        if (!expectValue(*arg)) {
            return ExpressionAnalysisResult::fail();
        }
    }
    if (expr.arguments.size() != 1) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    auto& arg = *expr.arguments.front();
    if (arg.type()->base() != sym.Int()) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    auto* elemType = sym.qualify(expr.object->type()->base(),
                                 TypeQualifiers::ImplicitReference);
    expr.decorate(nullptr, elemType, ast::ValueCategory::RValue);
    return ExpressionAnalysisResult::rvalue(elemType);
}

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
    // clang-format off
    return visit(*objRes.entity(), utl::overload{
        [&](OverloadSet& overloadSet) {
            auto* function = overloadSet.find(argTypes);
            if (!function) {
                iss.push<BadFunctionCall>(
                    fc,
                    objRes.entity()->symbolID(),
                    argTypes,
                    BadFunctionCall::Reason::NoMatchingFunction);
                return ExpressionAnalysisResult::fail();
            }
            fc.decorate(function,
                        function->signature().returnType(),
                        ast::ValueCategory::RValue);
            return ExpressionAnalysisResult::rvalue(fc.type());
        },
        [&](QualType& type) {
            Function* castFn = findExplicitCast(&type, argTypes);
            if (!castFn) {
                // TODO: Make better error class here.
                iss.push<BadTypeConversion>(*fc.arguments.front(), &type);
                return ExpressionAnalysisResult::fail();
            }
            fc.decorate(castFn, &type, ast::ValueCategory::RValue);
            return ExpressionAnalysisResult::rvalue(&type);
        },
        [&](Entity const& entity) {
            iss.push<BadFunctionCall>(
                fc,
                SymbolID::Invalid,
                argTypes,
                BadFunctionCall::Reason::ObjectNotCallable);
            return ExpressionAnalysisResult::fail();
        }
    }); // clang-format on
}

ExpressionAnalysisResult Context::analyze(ast::ListExpression& list) {
    for (auto* expr: list) {
        dispatch(*expr);
    }
    if (list.empty()) {
        return ExpressionAnalysisResult::indeterminate();
    }
    auto* first    = list.front();
    auto entityCat = first->entityCategory();
    switch (entityCat) {
    case ast::EntityCategory::Indeterminate:
        SC_DEBUGFAIL();
    case ast::EntityCategory::Value: {
        bool const allSameCat =
            ranges::all_of(list, [&](ast::Expression const* expr) {
                return expr->entityCategory() == entityCat;
            });
        if (!allSameCat) {
            iss.push<BadExpression>(list, IssueSeverity::Error);
        }
        // TODO: Check for common type!
        auto* type = sym.qualify(first->type()->base(),
                                 sema::TypeQualifiers::Array,
                                 list.size());
        list.decorate(nullptr, type, ast::ValueCategory::RValue);
        return ExpressionAnalysisResult::rvalue(type);
    }
    case ast::EntityCategory::Type: {
        auto* elementType = cast<Type*>(first->entity());
        if (list.size() != 1 && list.size() != 2) {
            iss.push<BadExpression>(list, IssueSeverity::Error);
            return ExpressionAnalysisResult::fail();
        }
        size_t arraySize = QualType::DynamicArraySize;
        if (list.size() == 2) {
            auto* countLiteral = dyncast<ast::IntegerLiteral const*>(list[1]);
            if (!countLiteral) {
                iss.push<BadExpression>(*list[1], IssueSeverity::Error);
                return ExpressionAnalysisResult::fail();
            }
            arraySize = countLiteral->value().to<size_t>();
        }
        auto* arrayType =
            sym.qualify(elementType, TypeQualifiers::Array, arraySize);
        list.decorate(const_cast<QualType*>(arrayType),
                      nullptr,
                      ast::ValueCategory::None,
                      ast::EntityCategory::Type);
        return ExpressionAnalysisResult::type(arrayType);
    }
    default:
        SC_UNREACHABLE();
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
        return stripQualifiers(expr.lhs->type());

    case Remainder:
        if (!verifySame()) {
            return nullptr;
        }
        if (!verifyAnyOf(expr.lhs->type()->base(), { sym.Int() })) {
            return nullptr;
        }
        return stripQualifiers(expr.lhs->type());

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
        return stripQualifiers(expr.lhs->type());

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
        return stripQualifiers(expr.lhs->type());

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

Function* Context::findExplicitCast(QualType const* to,
                                    std::span<QualType const* const> from) {
    if (from.size() != 1) {
        return nullptr;
    }
    if (from.front()->base() == sym.Int() && to->base() == sym.Float()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::i64tof64));
    }
    if (from.front()->base() == sym.Float() && to->base() == sym.Int()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::f64toi64));
    }
    return nullptr;
}

bool Context::expectValue(ast::Expression const& expr) {
    if (expr.entityCategory() != ast::EntityCategory::Value) {
        iss.push<BadSymbolReference>(expr,
                                     expr.entityCategory(),
                                     ast::EntityCategory::Value);
        return false;
    }
    return true;
}
