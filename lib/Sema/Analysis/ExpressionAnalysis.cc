#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/Fwd.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/OverloadResolution.h"
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
    bool rewriteMemberCall(ast::FunctionCall&);
    bool analyzeImpl(ast::Subscript&);
    bool analyzeImpl(ast::GenericExpression&);
    bool analyzeImpl(ast::Conversion&);
    bool analyzeImpl(ast::ListExpression&);

    bool analyzeImpl(ast::AbstractSyntaxTree&) { SC_DEBUGFAIL(); }

    Entity* lookup(ast::Identifier& id);

    /// \Returns `true` iff \p expr is a value. Otherwise submits an error and
    /// returns `false`
    bool expectValue(ast::Expression const& expr);

    /// \Returns `true` iff \p expr is a type. Otherwise submits an error and
    /// returns `false`
    bool expectType(ast::Expression const& expr);

    QualType const* analyzeBinaryExpr(ast::BinaryExpression&);

    QualType const* analyzeReferenceAssignment(ast::BinaryExpression&);

    Function* findExplicitCast(ObjectType const* targetType,
                               std::span<QualType const* const> from);

    QualType const* stripQualifiers(QualType const* type) const {
        return sym.qualify(type->base());
    }

    QualType const* makeRefImplicit(QualType const* type) const {
        if (!type) {
            return nullptr;
        }
        if (!type->isReference()) {
            return type;
        }
        return sym.setReference(type, Reference::Implicit);
    }

    QualType const* removeReference(QualType const* type) const {
        return sym.setReference(type, Reference::None);
    }

    SymbolTable& sym;
    IssueHandler& iss;
    /// Will be set by MemberAccess when right hand side is an identifier and
    /// unset by Identifier
    bool performRestrictedNameLookup = false;
};

} // namespace

template <typename... Args, typename T>
static bool isAny(T const* t) {
    return (isa<Args>(t) || ...);
}

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
    case ast::LiteralKind::String: {
        lit.decorate(nullptr, sym.arrayView(sym.Byte()));
        return true;
    }
    }
}

bool Context::analyzeImpl(ast::UnaryPrefixExpression& u) {
    if (!analyze(*u.operand())) {
        return false;
    }
    auto const* operandType = u.operand()->type();
    auto* baseType          = operandType->base();
    switch (u.operation()) {
    case ast::UnaryPrefixOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Negation:
        if (!isAny<IntType, FloatType>(baseType)) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(baseType)) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::LogicalNot:
        if (!isAny<BoolType>(baseType)) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::Increment:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Decrement:
        if (!isAny<IntType>(baseType)) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        if (!convertToImplicitRef(u.operand(), sym, iss)) {
            return false;
        }
        break;
    case ast::UnaryPrefixOperator::_count:
        SC_UNREACHABLE();
    }
    u.decorate(nullptr, sym.stripQualifiers(operandType));
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
    Entity* entity = lookup(id);
    if (!entity) {
        return false;
    }
    // clang-format off
    return visit(*entity, utl::overload{
        [&](Variable& var) {
            id.decorate(&var, makeRefImplicit(var.type()));
            return true;
        },
        [&](ObjectType const& type) {
            auto* qualType = sym.qualify(&type);
            id.decorate(const_cast<QualType*>(qualType), nullptr);
            return true;
        },
        [&](OverloadSet& overloadSet) {
            id.decorate(&overloadSet);
            return true;
        },
        [&](Generic& generic) {
            id.decorate(&generic, nullptr);
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
    /// Dereference the object if its a value
    if (ma.object()->isValue()) {
        dereference(ma.object(), sym);
    }
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
    SC_ASSERT(!isa<ast::FunctionCall>(ma.parent()), "Precondition");
    /// We reference an overload set, so since our parent is not a call
    /// expression we rewrite the AST here
    auto* overloadSet = cast<OverloadSet*>(ma.member()->entity());
    auto* argType = sym.setReference(ma.object()->type(), Reference::Explicit);
    auto funcRes =
        performOverloadResolution(overloadSet, std::array{ argType }, true);
    if (!funcRes) {
        funcRes.error()->setSourceRange(ma.sourceRange());
        iss.push(funcRes.error());
        return false;
    }
    auto* func = *funcRes;
    utl::small_vector<UniquePtr<ast::Expression>> args;
    args.push_back(ma.extractObject());
    auto call = allocate<ast::FunctionCall>(ma.extractMember(),
                                            std::move(args),
                                            ma.sourceRange());
    call->decorate(func,
                   makeRefImplicit(func->returnType()),
                   ValueCategory::RValue);

    bool const convSucc =
        convertExplicitly(call->argument(0), makeRefImplicit(argType), iss);
    SC_ASSERT(convSucc,
              "If overload resolution succeeds conversion must not fail");

    /// Now `ma` goes out of scope
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
        auto* refType = sym.setReference(referred.type(), Reference::Explicit);
        ref.decorate(referred.entity(), refType, ValueCategory::RValue);
        return true;
    }
    case EntityCategory::Type: {
        auto* qualType = cast<QualType const*>(referred.entity());
        auto* refType  = sym.setReference(qualType, Reference::Explicit);
        ref.decorate(const_cast<QualType*>(refType));
        return true;
    }
    default:
        SC_DEBUGFAIL();
    }
}

bool Context::analyzeImpl(ast::UniqueExpression& expr) {
    SC_DEBUGFAIL();
    // auto* initExpr = dyncast<ast::FunctionCall*>(expr.initExpr());
    // if (!initExpr) {
    //     iss.push<BadExpression>(*expr.initExpr(), IssueSeverity::Error);
    //     return false;
    // }
    // auto* typeExpr = initExpr->object();
    // if (!analyze(*typeExpr) || !typeExpr->isType()) {
    //     iss.push<BadExpression>(*typeExpr, IssueSeverity::Error);
    //     return false;
    // }
    // SC_ASSERT(initExpr->arguments().size() == 1, "Implement this properly");
    // auto* argument = initExpr->arguments().front();
    // if (!analyze(*argument) || !argument->isValue()) {
    //     iss.push<BadExpression>(*argument, IssueSeverity::Error);
    //     return false;
    // }
    // auto* rawType = cast<Type const*>(typeExpr->entity());
    // auto* type =
    //     sym.qualify(rawType, ExplicitReference | TypeQualifiers::Unique);
    // expr.decorate(type);
    // return true;
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
    for (auto* arg: expr.arguments()) {
        success &= analyze(*arg);
        success &= expectValue(*arg);
    }
    if (!success) {
        return false;
    }
    auto* arrayType = dyncast<ArrayType const*>(expr.object()->type()->base());
    if (!arrayType) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    if (expr.arguments().size() != 1) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    auto& arg = *expr.arguments().front();
    if (!isa<IntType>(arg.type()->base())) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    dereference(expr.object(), sym);
    dereference(expr.argument(0), sym);
    auto* elemType = sym.qualify(arrayType->elementType(), Reference::Implicit);
    expr.decorate(nullptr, elemType);
    return true;
}

bool Context::analyzeImpl(ast::GenericExpression& expr) {
    bool success = analyze(*expr.object());
    for (auto* arg: expr.arguments()) {
        success &= analyze(*arg);
    }
    if (!success) {
        return false;
    }
    SC_ASSERT(cast<ast::Identifier*>(expr.object())->value() == "reinterpret",
              "For now");
    SC_ASSERT(expr.arguments().size() == 1, "For now");
    SC_ASSERT(expr.argument(0)->isType(), "For now");
    expr.decorate(nullptr, cast<QualType const*>(expr.argument(0)->entity()));
    return true;
}

bool Context::analyzeImpl(ast::FunctionCall& fc) {
    bool success      = analyze(*fc.object());
    bool isMemberCall = false;
    /// We analyze all the arguments
    for (auto* arg: fc.arguments()) {
        success &= analyze(*arg);
    }
    if (!success) {
        return false;
    }

    /// If our object is a member access expression, we rewrite the AST
    if (auto* ma = dyncast<ast::MemberAccess*>(fc.object());
        ma && ma->object()->isValue())
    {
        auto memberAccess = fc.extractObject<ast::MemberAccess>();
        auto memFunc      = memberAccess->extractMember();
        auto objectArg    = memberAccess->extractObject();
        fc.insertArgument(0, std::move(objectArg));
        fc.setObject(std::move(memFunc));
        isMemberCall = true;
        /// Member access object `object` goes out of scope and is destroyed
    }

    /// If our object is a generic expression, we assert that is a `reinterpret`
    /// expression and rewrite the AST
    if (auto* genExpr = dyncast<ast::GenericExpression*>(fc.object())) {
        SC_ASSERT(genExpr->object()->entity()->name() == "reinterpret", "");
        SC_ASSERT(fc.arguments().size() == 1, "");
        auto* targetType =
            cast<QualType const*>(genExpr->argument(0)->entity());
        auto* arg = fc.argument(0);
        fc.parent()->replaceChild(&fc, fc.extractArgument(0));
        return convertReinterpret(arg, targetType, iss);
    }

    /// if our object is a type, then we rewrite the AST so we end up with just
    /// a conversion node
    if (auto* targetType = dyncast<QualType const*>(fc.object()->entity())) {
        SC_ASSERT(fc.arguments().size() == 1, "For now...");
        auto* arg = fc.argument(0);
        fc.parent()->replaceChild(&fc, fc.extractArgument(0));
        return convertExplicitly(arg, targetType, iss);
    }

    /// Extract the argument types to perform overload resolution
    auto argTypes = fc.arguments() |
                    ranges::views::transform([](ast::Expression const* expr) {
                        return expr->type();
                    }) |
                    ranges::to<utl::small_vector<QualType const*>>;

    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast_or_null<OverloadSet*>(fc.object()->entity());
    if (!overloadSet) {
        iss.push<BadFunctionCall>(fc,
                                  nullptr,
                                  argTypes,
                                  BadFunctionCall::Reason::ObjectNotCallable);
        return false;
    }

    /// Perform overload resolution
    auto result =
        performOverloadResolution(overloadSet, argTypes, isMemberCall);
    if (!result) {
        auto* error = result.error();
        error->setSourceRange(fc.sourceRange());
        iss.push(error);
        return false;
    }
    auto* function = result.value();
    fc.decorate(function,
                makeRefImplicit(function->returnType()),
                ValueCategory::RValue);

    /// We issue conversions for our arguments as necessary
    for (auto [index, arg, targetType]:
         ranges::views::zip(ranges::views::iota(0),
                            fc.arguments(),
                            function->argumentTypes()))
    {
        [[maybe_unused]] bool success = true;
        if (index == 0 && isMemberCall) {
            success = convertExplicitly(arg, makeRefImplicit(targetType), iss);
        }
        else {
            success = convertImplicitly(arg, targetType, iss);
        }
        SC_ASSERT(
            success,
            "Called function should not have been selected by overload "
            "resolution if the implicit conversion of the argument fails");
    }

    return true;
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

static std::optional<ObjectType const*> getResultType(SymbolTable& sym,
                                                      ObjectType const* type,
                                                      ast::BinaryOperator op) {
    if (ast::isArithmeticAssignment(op)) {
        if (getResultType(sym, type, ast::toNonAssignment(op)).has_value()) {
            return sym.Void();
        }
        return std::nullopt;
    }
    switch (op) {
        using enum ast::BinaryOperator;
    case Multiplication:
        [[fallthrough]];
    case Division:
        [[fallthrough]];
    case Addition:
        [[fallthrough]];
    case Subtraction:
        if (isAny<IntType, FloatType>(type)) {
            return type;
        }
        return std::nullopt;

    case Remainder:
        if (isAny<IntType>(type)) {
            return type;
        }
        return std::nullopt;

    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr:
        if (isAny<ByteType, BoolType, IntType>(type)) {
            return type;
        }
        return std::nullopt;

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        if (isAny<ByteType, IntType, FloatType>(type)) {
            return sym.Bool();
        }
        return std::nullopt;

    case Equals:
        [[fallthrough]];
    case NotEquals:
        if (isAny<ByteType, BoolType, IntType, FloatType>(type)) {
            return sym.Bool();
        }
        return std::nullopt;

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr:
        if (isAny<BoolType>(type)) {
            return sym.Bool();
        }
        return std::nullopt;

    case LeftShift:
        [[fallthrough]];
    case RightShift:
        if (isAny<ByteType, IntType>(type)) {
            return type;
        }
        return std::nullopt;

    case Assignment:
        return sym.Void();

    default:
        SC_UNREACHABLE();
    }
}

QualType const* Context::analyzeBinaryExpr(ast::BinaryExpression& expr) {
    /// Handle comma operator separately
    if (expr.operation() == ast::BinaryOperator::Comma) {
        return expr.rhs()->type();
    }
    /// Also reference assignment
    if (expr.operation() == ast::BinaryOperator::Assignment &&
        expr.rhs()->type()->isExplicitReference())
    {
        return analyzeReferenceAssignment(expr);
    }

    /// Determine common type
    auto* const commonQualifiedType =
        sema::commonType(expr.lhs()->type(), expr.rhs()->type());
    if (!commonQualifiedType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return nullptr;
    }
    auto* const commonType = sym.stripQualifiers(commonQualifiedType);

    auto resultType = getResultType(sym, commonType->base(), expr.operation());
    if (!resultType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return nullptr;
    }

    /// Handle value assignment seperately
    if (ast::isAssignment(expr.operation())) {
        auto* lhsType = expr.lhs()->type();
        /// Here we only look at assignment _through_ references
        /// That means LHS shall be an implicit reference
        bool success = true;
        success &= convertToImplicitRef(expr.lhs(), sym, iss);
        success &= convertImplicitly(expr.rhs(), stripQualifiers(lhsType), iss);
        return success ? sym.qVoid() : nullptr;
    }

    bool successfullConv = true;
    successfullConv &= convertImplicitly(expr.lhs(), commonType, iss);
    successfullConv &= convertImplicitly(expr.rhs(), commonType, iss);

    if (successfullConv) {
        return sym.qualify(*resultType);
    }
    return nullptr;
}

QualType const* Context::analyzeReferenceAssignment(
    ast::BinaryExpression& expr) {
    SC_ASSERT(expr.rhs()->type()->isExplicitReference(), "");
    auto* explicitRefType =
        sym.setReference(expr.lhs()->type(), Reference::Explicit);
    bool success = true;
    success &= convertExplicitly(expr.lhs(), explicitRefType, iss);
    success &= convertExplicitly(expr.rhs(), explicitRefType, iss);
    return success ? sym.qVoid() : nullptr;
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

Entity* Context::lookup(ast::Identifier& id) {
    bool const restrictedLookup = performRestrictedNameLookup;
    performRestrictedNameLookup = false;
    auto* entity                = restrictedLookup ?
                                      sym.currentScope().findEntity(id.value()) :
                                      sym.lookup(id.value());
    if (!entity && !restrictedLookup) {
        /// We don't emit issues when performing restricted lookup because we
        /// may continue searching other scopes
        iss.push<UseOfUndeclaredIdentifier>(id, sym.currentScope());
    }
    return entity;
}

bool Context::expectValue(ast::Expression const& expr) {
    if (!expr.isDecorated()) {
        return false;
    }
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
