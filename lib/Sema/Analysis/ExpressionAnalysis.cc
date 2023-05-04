#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/Fwd.h"
#include "Sema/Conversion.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    bool analyze(ast::Expression&);

    bool analyzeImpl(ast::Literal&);
    bool analyzeImpl(ast::UnaryPrefixExpression&);
    bool analyzeImpl(ast::BinaryExpression&);

    bool analyzeImpl(ast::Identifier&);
    bool analyzeImpl(ast::MemberAccess&);
    bool uniformFunctionCall(ast::MemberAccess&);
    bool rewritePropertyCall(ast::MemberAccess&);
    bool analyzeImpl(ast::ReferenceExpression&);
    bool analyzeImpl(ast::UniqueExpression&);
    bool analyzeImpl(ast::Conditional&);
    bool analyzeImpl(ast::FunctionCall&);
    bool analyzeImpl(ast::Subscript&);
    bool analyzeImpl(ast::Conversion&);
    bool analyzeImpl(ast::ListExpression&);

    bool analyzeImpl(ast::AbstractSyntaxTree&) { SC_DEBUGFAIL(); }

    /// \Returns `true` iff \p expr is a value. Otherwise submits an error and
    /// returns `false`
    bool expectValue(ast::Expression const& expr);

    /// \Returns `true` iff \p expr is a type. Otherwise submits an error and
    /// returns `false`
    bool expectType(ast::Expression const& expr);

    QualType const* analyzeBinaryExpr(ast::BinaryExpression&);

    Function* findExplicitCast(ObjectType const* targetType,
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

bool sema::analyzeExpression(ast::Expression& expr,
                             SymbolTable& sym,
                             IssueHandler& iss) {
    Context ctx{ .sym = sym, .iss = iss };
    return ctx.analyze(expr);
}

bool Context::analyze(ast::Expression& expr) {
    if (expr.isDecorated()) {
        return true;
    }
    return visit(expr, [this](auto&& e) { return this->analyzeImpl(e); });
}

bool Context::analyzeImpl(ast::Literal& lit) {
    switch (lit.kind()) {
    case ast::LiteralKind::Integer:
        lit.decorate(nullptr, sym.qS64());
        return true;
    case ast::LiteralKind::Boolean:
        lit.decorate(nullptr, sym.qBool());
        return true;
    case ast::LiteralKind::FloatingPoint:
        lit.decorate(nullptr, sym.qFloat());
        return true;
    case ast::LiteralKind::This: {
        auto* scope = &sym.currentScope();
        while (!isa<Function>(scope)) {
            scope = scope->parent();
        }
        auto* function = cast<Function*>(scope);
        if (!function->isMember()) {
            iss.push<BadExpression>(lit, IssueSeverity::Error);
            return false;
        }
        auto* type       = function->argumentTypes().front();
        auto* thisEntity = function->findEntity<Variable>("__this");
        lit.decorate(thisEntity, type);
        return true;
    }
    case ast::LiteralKind::String:
        lit.decorate(nullptr, sym.qString());
        return true;
    }
}

bool Context::analyzeImpl(ast::UnaryPrefixExpression& u) {
    if (!analyze(*u.operand())) {
        return false;
    }
    auto const* operandType = u.operand()->type();
    auto submitIssue        = [&] {
        iss.push<BadOperandForUnaryExpression>(u, operandType);
    };
    switch (u.operation()) {
    case ast::UnaryPrefixOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Negation:
        if (operandType->base() != sym.S64() &&
            operandType->base() != sym.Float())
        {
            submitIssue();
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::BitwiseNot:
        if (operandType->base() != sym.S64()) {
            submitIssue();
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::LogicalNot:
        if (operandType->base() != sym.Bool()) {
            submitIssue();
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::Increment:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Decrement:
        // TODO: Check for mutability
        if (operandType->base() != sym.S64()) {
            submitIssue();
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::_count:
        SC_DEBUGFAIL();
    }
    u.decorate(nullptr, sym.qualify(u.operand()->type()->base()));
    return true;
}

bool Context::analyzeImpl(ast::BinaryExpression& b) {
    if (!analyze(*b.lhs()) || !analyze(*b.rhs())) {
        return false;
    }
    auto* resultType = analyzeBinaryExpr(b);
    if (!resultType) {
        return false;
    }
    b.decorate(nullptr, resultType);
    return true;
}

bool Context::analyzeImpl(ast::Identifier& id) {
    bool const restrictedLookup = performRestrictedNameLookup;
    performRestrictedNameLookup = false;
    Entity* entity              = [&] {
        if (restrictedLookup) {
            /// When we are on the right hand side of a member access expression
            /// we restrict lookup to the scope of the object of the left hand
            /// side.
            return sym.currentScope().findEntity(id.value());
        }
        else {
            return sym.lookup(id.value());
        }
    }();
    if (!entity) {
        if (!restrictedLookup) {
            iss.push<UseOfUndeclaredIdentifier>(id, sym.currentScope());
        }
        return false;
    }
    // clang-format off
    return visit(*entity, utl::overload{
        [&](Variable& var) {
            id.decorate(&var, var.type());
            return true;
        },
        [&](ObjectType const& type) {
            auto* qualType = sym.qualify(&type);
            id.decorate(const_cast<QualType*>(qualType),
                        nullptr);
            return true;
        },
        [&](OverloadSet& overloadSet) {
            id.decorate(&overloadSet);
            return true;
        },
        [&](Entity const& entity) -> bool {
            SC_DEBUGFAIL(); // Maybe push an issue here?
        }
    }); // clang-format on
}

bool Context::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(*ma.object())) {
        return false;
    }
    Scope* lookupTargetScope =
        const_cast<ObjectType*>(ma.object()->typeOrTypeEntity()->base());
    SC_ASSERT(lookupTargetScope, "analyze(ma.object()) should have failed");
    bool success = sym.withScopeCurrent(lookupTargetScope, [&] {
        /// We restrict name lookup to the
        /// current scope. This flag will be unset by the identifier case.
        performRestrictedNameLookup = true;
        return analyze(*ma.member());
    });
    if (!success) {
        success = uniformFunctionCall(ma);
    }
    if (!success) {
        return false;
    }
    if (isa<OverloadSet>(ma.member()->entity()) &&
        !isa<ast::FunctionCall>(ma.parent()))
    {
        return rewritePropertyCall(ma);
    }
    if (ma.object()->isValue() && !ma.member()->isValue()) {
        SC_DEBUGFAIL(); /// Can't look in a value and then in a type. probably
                        /// just return failure here
        return false;
    }
    ma.decorate(ma.member()->entity(),
                ma.member()->type(),
                ma.object()->valueCategory(),
                ma.member()->entityCategory());
    return true;
}

bool Context::uniformFunctionCall(ast::MemberAccess& ma) {
    Scope* lookupTargetScope =
        const_cast<ObjectType*>(ma.object()->typeOrTypeEntity()->base());
    /// If we don't find a member and our object is a value, we will look
    /// for a function in the scope of the type of our object
    if (!ma.object()->isValue()) {
        /// Need to push the error here because the `Identifier` case does
        /// not push an error in member access lookup
        iss.push<UseOfUndeclaredIdentifier>(*ma.member(), *lookupTargetScope);
        return false;
    }
    bool success = sym.withScopeCurrent(lookupTargetScope->parent(),
                                        [&] { return analyze(*ma.member()); });
    if (!success) {
        return false;
    }
    auto* overloadSet = dyncast<OverloadSet*>(ma.member()->entity());
    if (!overloadSet) {
        iss.push<UseOfUndeclaredIdentifier>(*ma.member(), *lookupTargetScope);
        return false;
    }
    return true;
}

bool Context::rewritePropertyCall(ast::MemberAccess& ma) {
    /// We reference an overload set, so if our parent is not a call expression
    /// we should rewrite this
    auto* overloadSet = cast<OverloadSet*>(ma.member()->entity());
    auto* argType = sym.addQualifiers(ma.object()->type(), ExplicitReference);
    auto* func    = overloadSet->find(std::array{ argType });
    if (!func) {
        iss.push<BadExpression>(ma);
        return false;
    }
    auto sourceRange = ma.object()->sourceRange();
    auto refArg =
        allocate<ast::ReferenceExpression>(ma.extractObject(), sourceRange);
    if (!analyze(*refArg)) {
        return false;
    }
    utl::small_vector<UniquePtr<ast::Expression>> args;
    args.push_back(std::move(refArg));
    auto call = allocate<ast::FunctionCall>(ma.extractMember(),
                                            std::move(args),
                                            ma.sourceRange());
    call->decorate(func, func->returnType(), ValueCategory::RValue);
    ma.parent()->replaceChild(&ma, std::move(call));
    return true;
}

bool Context::analyzeImpl(ast::ReferenceExpression& ref) {
    if (!analyze(*ref.referred())) {
        return false;
    }
    auto& referred = *ref.referred();
    switch (referred.entityCategory()) {
    case EntityCategory::Value: {
        if (!referred.isLValue() && !referred.type()->isReference()) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return false;
        }
        auto* refType = sym.addQualifiers(referred.type(), ExplicitReference);
        ref.decorate(referred.entity(), refType, ValueCategory::RValue);
        return true;
    }
    case EntityCategory::Type: {
        auto* qualType = cast<QualType const*>(referred.entity());
        auto* refType  = sym.addQualifiers(qualType, ImplicitReference);
        ref.decorate(const_cast<QualType*>(refType));
        return true;
    }
    default:
        SC_DEBUGFAIL();
    }
}

bool Context::analyzeImpl(ast::UniqueExpression& expr) {
    auto* initExpr = dyncast<ast::FunctionCall*>(expr.initExpr());
    if (!initExpr) {
        iss.push<BadExpression>(*expr.initExpr(), IssueSeverity::Error);
        return false;
    }
    auto* typeExpr = initExpr->object();
    if (!analyze(*typeExpr) || !typeExpr->isType()) {
        iss.push<BadExpression>(*typeExpr, IssueSeverity::Error);
        return false;
    }
    SC_ASSERT(initExpr->arguments().size() == 1, "Implement this properly");
    auto* argument = initExpr->arguments().front();
    if (!analyze(*argument) || !argument->isValue()) {
        iss.push<BadExpression>(*argument, IssueSeverity::Error);
        return false;
    }
    auto* rawType = cast<Type const*>(typeExpr->entity());
    auto* type =
        sym.qualify(rawType, ExplicitReference | TypeQualifiers::Unique);
    expr.decorate(type);
    return true;
}

bool Context::analyzeImpl(ast::Conditional& c) {
    bool success = analyze(*c.condition());
    if (success) {
        convertImplicitly(c.condition(), sym.qBool(), iss);
    }
    success &= analyze(*c.thenExpr());
    success &= expectValue(*c.thenExpr());
    success &= analyze(*c.elseExpr());
    success &= expectValue(*c.elseExpr());
    if (!success) {
        return false;
    }
    auto* thenType = c.thenExpr()->type();
    auto* elseType = c.elseExpr()->type();
    if (thenType->base() != elseType->base()) {
        iss.push<BadOperandsForBinaryExpression>(c, thenType, elseType);
        return false;
    }
    // TODO: Return a reference here to allow conditional assignment etc.
    c.decorate(nullptr, thenType);
    return true;
}

bool Context::analyzeImpl(ast::Subscript& expr) {
    bool success = analyze(*expr.object());
    success &= expectValue(*expr.object());
    auto* arrayType = dyncast<ArrayType const*>(expr.object()->type()->base());
    if (!arrayType) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    for (auto* arg: expr.arguments()) {
        success &= analyze(*arg);
        success &= expectValue(*arg);
    }
    if (!success) {
        return false;
    }
    if (expr.arguments().size() != 1) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    auto& arg = *expr.arguments().front();
    if (arg.type()->base() != sym.S64()) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    auto* elemType = sym.qualify(arrayType->elementType(), ImplicitReference);
    expr.decorate(nullptr, elemType);
    return true;
}

bool Context::analyzeImpl(ast::FunctionCall& fc) {
    bool success = analyze(*fc.object());
    utl::small_vector<QualType const*> argTypes;
    argTypes.reserve(fc.arguments().size());
    for (auto* arg: fc.arguments()) {
        success &= analyze(*arg);
        /// `arg` is undecorated if analysis of `arg` failed.
        argTypes.push_back(arg->isDecorated() ? arg->type() : nullptr);
    }
    if (!success) {
        return false;
    }
    if (auto* memberAccess = dyncast<ast::MemberAccess*>(fc.object());
        memberAccess && memberAccess->object()->isValue())
    {
        auto qualType = sym.addQualifiers(memberAccess->object()->type(),
                                          TypeQualifiers::ExplicitReference);
        argTypes.insert(argTypes.begin(), qualType);
        fc.isMemberCall = true;
    }
    // clang-format off
    return visit(*fc.object()->entity(), utl::overload{
        [&](OverloadSet& overloadSet) {
            auto* function = overloadSet.find(argTypes);
            if (!function) {
                iss.push<BadFunctionCall>(
                    fc,
                    &overloadSet,
                    argTypes,
                    BadFunctionCall::Reason::NoMatchingFunction);
                return false;
            }
            fc.decorate(function,
                        function->returnType(),
                        ValueCategory::RValue);
            return true;
        },
        [&](QualType const& type) {
            Function* castFn = findExplicitCast(type.base(), argTypes);
            if (!castFn) {
                // TODO: Make better error class here.
                iss.push<BadTypeConversion>(*fc.arguments().front(), &type);
                return false;
            }
            fc.decorate(castFn, &type, ValueCategory::RValue);
            return true;
        },
        [&](Entity const& entity) {
            iss.push<BadFunctionCall>(
                fc,
                nullptr,
                argTypes,
                BadFunctionCall::Reason::ObjectNotCallable);
            return false;
        }
    }); // clang-format on
}

bool Context::analyzeImpl(ast::Conversion& conv) {
    SC_DEBUGFAIL();

    if (!analyze(*conv.expression())) {
        return false;
    }
    if (!expectValue(*conv.expression())) {
        return false;
    }
    auto* source = conv.expression()->type();
    auto* target = conv.targetType();
    if (source == target) {
        conv.decorate(conv.expression()->entity(), conv.targetType());
        return true;
    }
    if (source->base() == target->base()) {
        if (source->isReference() && !target->isReference()) {
            conv.decorate(conv.expression()->entity(), target);
            return true;
        }
        if (source->isExplicitReference() && target->isReference()) {
            conv.decorate(conv.expression()->entity(), source);
            return true;
        }
    }
    iss.push<BadTypeConversion>(*conv.expression(), target);
    return false;
}

bool Context::analyzeImpl(ast::ListExpression& list) {
    bool success = true;
    for (auto* expr: list.elements()) {
        success &= analyze(*expr);
    }
    if (!success) {
        return false;
    }
    if (list.elements().empty()) {
        list.decorate(nullptr,
                      nullptr,
                      std::nullopt,
                      EntityCategory::Indeterminate);
        return true;
    }
    auto* first    = list.elements().front();
    auto entityCat = first->entityCategory();
    switch (entityCat) {
    case EntityCategory::Value: {
        bool const allSameCat =
            ranges::all_of(list.elements(), [&](ast::Expression const* expr) {
                return expr->entityCategory() == entityCat;
            });
        if (!allSameCat) {
            iss.push<BadExpression>(list, IssueSeverity::Error);
        }
        // TODO: Check for common type!
        auto* arrayType =
            sym.arrayType(first->type()->base(), list.elements().size());
        auto* qualType = sym.qualify(arrayType);
        list.decorate(nullptr, qualType);
        return true;
    }
    case EntityCategory::Type: {
        auto* elementType = cast<QualType*>(first->entity())->base();
        if (list.elements().size() != 1 && list.elements().size() != 2) {
            iss.push<BadExpression>(list, IssueSeverity::Error);
            return false;
        }
        size_t count = ArrayType::DynamicCount;
        if (list.elements().size() == 2) {
            auto* countLiteral =
                dyncast<ast::Literal const*>(list.elements()[1]);
            if (!countLiteral ||
                countLiteral->kind() != ast::LiteralKind::Integer)
            {
                iss.push<BadExpression>(*list.elements()[1],
                                        IssueSeverity::Error);
                return false;
            }
            count =
                countLiteral->value<ast::LiteralKind::Integer>().to<size_t>();
        }
        auto* arrayType = sym.arrayType(elementType, count);
        auto* qualType  = sym.qualify(arrayType);
        list.decorate(const_cast<QualType*>(qualType), nullptr);
        return true;
    }
    default:
        SC_UNREACHABLE();
    }
}

QualType const* Context::analyzeBinaryExpr(ast::BinaryExpression& expr) {
    if (expr.operation() == ast::BinaryOperator::Comma) {
        return expr.rhs()->type();
    }

    auto* commonType = sema::commonType(expr.lhs()->type(), expr.rhs()->type());

    auto submitIssue = [&] {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
    };
    auto verifyAnyOf =
        [&](QualType const* toCheck, std::initializer_list<Type const*> types) {
        bool result = false;
        for (auto type: types) {
            if (toCheck->base() == type) {
                result = true;
            }
        }
        if (!result) {
            submitIssue();
            return false;
        }
        return true;
    };
    auto convertOperands = [&]() -> bool {
        bool result = true;
        result &= convertImplicitly(expr.lhs(), commonType, iss);
        result &= convertImplicitly(expr.rhs(), commonType, iss);
        return result;
    };

    if (!commonType) {
        submitIssue();
        return nullptr;
    }

    switch (expr.operation()) {
        using enum ast::BinaryOperator;
    case Multiplication:
        [[fallthrough]];
    case Division:
        [[fallthrough]];
    case Addition:
        [[fallthrough]];
    case Subtraction:
        if (!verifyAnyOf(commonType, { sym.S64(), sym.Float() })) {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return stripQualifiers(commonType);

    case Remainder:
        if (!verifyAnyOf(commonType, { sym.S64() })) {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return stripQualifiers(commonType);

    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr:
        if (!verifyAnyOf(commonType, { sym.Byte(), sym.Bool(), sym.S64() })) {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return stripQualifiers(commonType);

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        if (!verifyAnyOf(commonType, { sym.Byte(), sym.S64(), sym.Float() })) {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return sym.qBool();

    case Equals:
        [[fallthrough]];
    case NotEquals:
        if (!verifyAnyOf(commonType,
                         { sym.Byte(), sym.Bool(), sym.S64(), sym.Float() }))
        {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return sym.qBool();

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr:
        if (!verifyAnyOf(commonType, { sym.Bool() })) {
            return nullptr;
        }
        convertOperands();
        return sym.qBool();

    case LeftShift:
        [[fallthrough]];
    case RightShift:
        if (!verifyAnyOf(commonType, { sym.S64() })) {
            return nullptr;
        }
        if (!convertOperands()) {
            return nullptr;
        }
        return stripQualifiers(commonType);

    case Assignment:
        /// Reference reassignment case
        if (expr.rhs()->type()->isExplicitReference() &&
            isImplicitlyConvertible(expr.lhs()->type(), expr.rhs()->type()))
        {
            insertConversion(expr.rhs(), expr.lhs()->type());
            return sym.qVoid();
        }
        [[fallthrough]];
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
    case XOrAssignment: {
        /// Here we only look at assignment _through_ references, so we strip
        /// the reference qualifier
        auto* toType         = expr.lhs()->type();
        auto* toTypeStripped = stripQualifiers(toType);
        auto* fromType       = expr.rhs()->type();
        if (!expr.lhs()->isLValue() && !toType->isReference()) {
            return nullptr;
        }
        if (fromType == toTypeStripped) {
            return sym.qVoid();
        }
        if (!convertImplicitly(expr.rhs(), toTypeStripped, iss)) {
            return nullptr;
        }
        return sym.qVoid();
    }
    case Comma:
        SC_UNREACHABLE("Handled above");

    case _count:
        SC_DEBUGFAIL();
    }
}

Function* Context::findExplicitCast(ObjectType const* to,
                                    std::span<QualType const* const> from) {
    if (from.size() != 1) {
        return nullptr;
    }
    if (from.front()->base() == sym.S64() && to == sym.Float()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::i64tof64));
    }
    if (from.front()->base() == sym.Float() && to == sym.S64()) {
        return sym.builtinFunction(static_cast<size_t>(svm::Builtin::f64toi64));
    }
    return nullptr;
}

bool Context::expectValue(ast::Expression const& expr) {
    if (!expr.isValue()) {
        iss.push<BadSymbolReference>(expr, EntityCategory::Value);
        return false;
    }
    return true;
}

bool Context::expectType(ast::Expression const& expr) {
    if (!expr.isType()) {
        iss.push<BadSymbolReference>(expr, EntityCategory::Type);
        return false;
    }
    return true;
}
