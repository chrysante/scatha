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
    ExpressionAnalysisResult analyze(ast::Expression&);

    ExpressionAnalysisResult analyzeImpl(ast::IntegerLiteral&);
    ExpressionAnalysisResult analyzeImpl(ast::BooleanLiteral&);
    ExpressionAnalysisResult analyzeImpl(ast::FloatingPointLiteral&);
    ExpressionAnalysisResult analyzeImpl(ast::StringLiteral&);
    ExpressionAnalysisResult analyzeImpl(ast::UnaryPrefixExpression&);
    ExpressionAnalysisResult analyzeImpl(ast::BinaryExpression&);

    ExpressionAnalysisResult analyzeImpl(ast::Identifier&);
    ExpressionAnalysisResult analyzeImpl(ast::MemberAccess&);
    ExpressionAnalysisResult analyzeImpl(ast::ReferenceExpression&);
    ExpressionAnalysisResult analyzeImpl(ast::UniqueExpression&);
    ExpressionAnalysisResult analyzeImpl(ast::Conditional&);
    ExpressionAnalysisResult analyzeImpl(ast::FunctionCall&);
    ExpressionAnalysisResult analyzeImpl(ast::Subscript&);
    ExpressionAnalysisResult analyzeImpl(ast::ListExpression&);

    ExpressionAnalysisResult analyzeImpl(ast::AbstractSyntaxTree&) {
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
    return ctx.analyze(expr);
}

ExpressionAnalysisResult Context::analyze(ast::Expression& expr) {
    return visit(expr, [this](auto&& e) { return this->analyzeImpl(e); });
}

ExpressionAnalysisResult Context::analyzeImpl(ast::IntegerLiteral& l) {
    auto* type = sym.qualInt();
    l.decorate(nullptr, type);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::BooleanLiteral& l) {
    auto* type = sym.qualBool();
    l.decorate(nullptr, type);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::FloatingPointLiteral& l) {
    auto* type = sym.qualFloat();
    l.decorate(nullptr, type);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::StringLiteral& l) {
    auto* type = sym.qualString();
    l.decorate(nullptr, type);
    return ExpressionAnalysisResult::rvalue(type);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::UnaryPrefixExpression& u) {
    auto const opResult = analyze(*u.operand);
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
    u.decorate(nullptr, sym.qualify(u.operand->type()));
    return ExpressionAnalysisResult::rvalue(u.type());
}

ExpressionAnalysisResult Context::analyzeImpl(ast::BinaryExpression& b) {
    auto const lhsRes = analyze(*b.lhs);
    auto const rhsRes = analyze(*b.rhs);
    if (!lhsRes || !rhsRes) {
        return ExpressionAnalysisResult::fail();
    }
    auto* resultType = binaryOpResult(b);
    if (!resultType) {
        return ExpressionAnalysisResult::fail();
    }
    b.decorate(nullptr, resultType);
    return ExpressionAnalysisResult::rvalue(b.type());
}

ExpressionAnalysisResult Context::analyzeImpl(ast::Identifier& id) {
    Entity* entity = [&] {
        if (performRestrictedNameLookup) {
            /// When we are on the right hand side of a member access expression
            /// we restrict lookup to the scope of the object of the left hand
            /// side.
            performRestrictedNameLookup = false;
            return sym.currentScope().findEntity(id.value());
        }
        else {
            return sym.lookup(id.value());
        }
    }();
    if (!entity) {
        iss.push<UseOfUndeclaredIdentifier>(id, sym.currentScope());
        return ExpressionAnalysisResult::fail();
    }
    // clang-format off
    return visit(*entity, utl::overload{
        [&](Variable& var) {
            id.decorate(&var, var.type());
            return ExpressionAnalysisResult::lvalue(&var, var.type());
        },
        [&](Type& type) {
            id.decorate(&type,
                        nullptr);
            return ExpressionAnalysisResult::type(sym.qualify(&type));
        },
        [&](OverloadSet& overloadSet) {
            id.decorate(&overloadSet);
            return ExpressionAnalysisResult::lvalue(&overloadSet, nullptr);
        },
        [&](Entity const& entity) -> ExpressionAnalysisResult {
            SC_DEBUGFAIL(); // Maybe push an issue here?
        }
    }); // clang-format on
}

ExpressionAnalysisResult Context::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(*ma.object)) {
        return ExpressionAnalysisResult::fail();
    }
    Scope const* lookupTargetScope = ma.object->typeBaseOrTypeEntity();
    SC_ASSERT(lookupTargetScope, "analyze(ma.object) should have failed");
    auto* oldScope = &sym.currentScope();
    sym.makeScopeCurrent(const_cast<Scope*>(lookupTargetScope));
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(oldScope); };
    /// We restrict name lookup to the
    /// current scope. This flag will be unset by the identifier case.
    performRestrictedNameLookup = true;
    auto const memRes           = analyze(*ma.member);
    if (!memRes) {
        return ExpressionAnalysisResult::fail();
    }
    popScope.execute();
    if (ma.object->isValue() && !ma.member->isValue()) {
        SC_DEBUGFAIL(); /// Can't look in a value and then in a type. probably
                        /// just return failure here
        return ExpressionAnalysisResult::fail();
    }
    ma.decorate(ma.member->entity(),
                ma.member->type(),
                ma.object->valueCategory(),
                ma.member->entityCategory());
    return memRes;
}

ExpressionAnalysisResult Context::analyzeImpl(ast::ReferenceExpression& ref) {
    if (!analyze(*ref.referred)) {
        return ExpressionAnalysisResult::fail();
    }
    auto& referred = *ref.referred;
    if (referred.isValue()) {
        if (!referred.isLValue() && !referred.type()->isReference()) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return ExpressionAnalysisResult::fail();
        }
        auto* refType =
            sym.qualify(referred.type(), TypeQualifiers::ExplicitReference);
        ref.decorate(referred.entity(), refType);
        return ExpressionAnalysisResult::lvalue(referred.entity(), refType);
    }
    else {
        auto* refType = sym.qualify(cast<Type const*>(referred.entity()),
                                    TypeQualifiers::ImplicitReference);
        ref.decorate(const_cast<QualType*>(refType));
        return ExpressionAnalysisResult::type(refType);
    }
}

ExpressionAnalysisResult Context::analyzeImpl(ast::UniqueExpression& expr) {
    auto* initExpr = dyncast<ast::FunctionCall*>(expr.initExpr.get());
    if (!initExpr) {
        iss.push<BadExpression>(*expr.initExpr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    auto* typeExpr   = initExpr->object.get();
    auto typeExprRes = analyze(*typeExpr);
    if (!typeExprRes || typeExprRes.category() != EntityCategory::Type) {
        iss.push<BadExpression>(*typeExpr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    SC_ASSERT(initExpr->arguments.size() == 1, "Implement this properly");
    auto* argument = initExpr->arguments.front().get();
    auto argRes    = analyze(*argument);
    if (!argRes || argRes.category() != EntityCategory::Value) {
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

ExpressionAnalysisResult Context::analyzeImpl(ast::Conditional& c) {
    analyze(*c.condition);
    verifyConversion(*c.condition, sym.qualify(sym.Bool()));
    if (!analyze(*c.ifExpr) || !analyze(*c.elseExpr)) {
        return ExpressionAnalysisResult::fail();
    }
    if (!c.ifExpr->isValue()) {
        iss.push<BadSymbolReference>(*c.ifExpr,
                                     c.ifExpr->entityCategory(),
                                     EntityCategory::Value);
        return ExpressionAnalysisResult::fail();
    }
    if (!c.elseExpr->isValue()) {
        iss.push<BadSymbolReference>(*c.elseExpr,
                                     c.elseExpr->entityCategory(),
                                     EntityCategory::Value);
        return ExpressionAnalysisResult::fail();
    }
    auto* thenType = c.ifExpr->type();
    auto* elseType = c.elseExpr->type();
    if (thenType->base() != elseType->base()) {
        iss.push<BadOperandsForBinaryExpression>(c, thenType, elseType);
        return ExpressionAnalysisResult::fail();
    }
    // TODO: Return a reference here to allow conditional assignment etc.
    c.decorate(nullptr, thenType);
    return ExpressionAnalysisResult::rvalue(thenType);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::Subscript& expr) {
    bool analysisSuccess = (bool)analyze(*expr.object);
    if (!expectValue(*expr.object)) {
        return ExpressionAnalysisResult::fail();
    }
    if (!expr.object->type()->isArray()) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return ExpressionAnalysisResult::fail();
    }
    for (auto& arg: expr.arguments) {
        analysisSuccess |= (bool)analyze(*arg);
        if (!expectValue(*arg)) {
            return ExpressionAnalysisResult::fail();
        }
    }
    if (!analysisSuccess) {
        return ExpressionAnalysisResult::fail();
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
    expr.decorate(nullptr, elemType);
    return ExpressionAnalysisResult::rvalue(elemType);
}

ExpressionAnalysisResult Context::analyzeImpl(ast::FunctionCall& fc) {
    bool success = true;
    utl::small_vector<QualType const*> argTypes;
    argTypes.reserve(fc.arguments.size());
    for (auto& arg: fc.arguments) {
        auto const argRes = analyze(*arg);
        if (iss.fatal()) {
            return ExpressionAnalysisResult::fail();
        }
        success &= argRes.success();
        /// `arg` is undecorated if analysis of `arg` failed.
        argTypes.push_back(arg->isDecorated() ? arg->type() : nullptr);
    }
    auto const objRes = analyze(*fc.object);
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
                    &overloadSet,
                    argTypes,
                    BadFunctionCall::Reason::NoMatchingFunction);
                return ExpressionAnalysisResult::fail();
            }
            fc.decorate(function,
                        function->signature().returnType(),
                        ValueCategory::RValue);
            return ExpressionAnalysisResult::rvalue(fc.type());
        },
        [&](QualType& type) {
            Function* castFn = findExplicitCast(&type, argTypes);
            if (!castFn) {
                // TODO: Make better error class here.
                iss.push<BadTypeConversion>(*fc.arguments.front(), &type);
                return ExpressionAnalysisResult::fail();
            }
            fc.decorate(castFn, &type, ValueCategory::RValue);
            return ExpressionAnalysisResult::rvalue(&type);
        },
        [&](Entity const& entity) {
            iss.push<BadFunctionCall>(
                fc,
                nullptr,
                argTypes,
                BadFunctionCall::Reason::ObjectNotCallable);
            return ExpressionAnalysisResult::fail();
        }
    }); // clang-format on
}

ExpressionAnalysisResult Context::analyzeImpl(ast::ListExpression& list) {
    for (auto* expr: list) {
        analyze(*expr);
    }
    if (list.empty()) {
        return ExpressionAnalysisResult::indeterminate();
    }
    auto* first    = list.front();
    auto entityCat = first->entityCategory();
    switch (entityCat) {
    case EntityCategory::Indeterminate:
        SC_DEBUGFAIL();
    case EntityCategory::Value: {
        bool const allSameCat =
            ranges::all_of(list, [&](ast::Expression const* expr) {
                return expr->entityCategory() == entityCat;
            });
        if (!allSameCat) {
            iss.push<BadExpression>(list, IssueSeverity::Error);
        }
        // TODO: Check for common type!
        auto* type = sym.qualify(first->type()->base(),
                                 TypeQualifiers::Array,
                                 list.size());
        list.decorate(nullptr, type);
        return ExpressionAnalysisResult::rvalue(type);
    }
    case EntityCategory::Type: {
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
        list.decorate(const_cast<QualType*>(arrayType), nullptr);
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
    if (expr.entityCategory() != EntityCategory::Value) {
        iss.push<BadSymbolReference>(expr,
                                     expr.entityCategory(),
                                     EntityCategory::Value);
        return false;
    }
    return true;
}
