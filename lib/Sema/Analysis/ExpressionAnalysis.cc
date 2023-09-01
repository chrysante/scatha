#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/Fwd.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    bool analyze(ast::Expression&);

    bool analyzeImpl(ast::Literal&);
    bool analyzeImpl(ast::UnaryExpression&);
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
    bool analyzeImpl(ast::ListExpression&);

    bool analyzeImpl(ast::AbstractSyntaxTree&) { SC_UNREACHABLE(); }

    Entity* lookup(ast::Identifier& id);

    /// \Returns `true` iff \p expr is a value. Otherwise submits an error and
    /// returns `false`
    bool expectValue(ast::Expression const& expr);

    /// \Returns `true` iff \p expr is a type. Otherwise submits an error and
    /// returns `false`
    bool expectType(ast::Expression const& expr);

    QualType analyzeBinaryExpr(ast::BinaryExpression&);

    QualType analyzeReferenceAssignment(ast::BinaryExpression&);

    QualType makeRefImplicit(QualType type) const {
        if (!type) {
            return nullptr;
        }
        if (isRef(type)) {
            return sym.implRef(type);
        }
        return type;
    }

    QualType makeRefExplicit(QualType type) const {
        if (!type) {
            return nullptr;
        }
        SC_ASSERT(isRef(type), "???");
        return sym.explRef(type);
    }

    DTorStack& dtorStack;
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
                             DTorStack& dtorStack,
                             SymbolTable& sym,
                             IssueHandler& iss) {
    Context ctx{ .dtorStack = dtorStack, .sym = sym, .iss = iss };
    return ctx.analyze(expr);
}

bool Context::analyze(ast::Expression& expr) {
    if (expr.isDecorated()) {
        return true;
    }
    return visit(expr, [this](auto&& e) { return this->analyzeImpl(e); });
}

bool Context::analyzeImpl(ast::Literal& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        lit.decorate(nullptr, sym.S64());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ true));
        return true;

    case Boolean:
        lit.decorate(nullptr, sym.Bool());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return true;

    case FloatingPoint:
        lit.decorate(nullptr, sym.F64());
        lit.setConstantValue(allocate<FloatValue>(lit.value<APFloat>()));
        return true;

    case This: {
        auto* scope = &sym.currentScope();
        while (!isa<Function>(scope)) {
            scope = scope->parent();
        }
        auto* function = cast<Function*>(scope);
        if (!function->isMember()) {
            iss.push<BadExpression>(lit, IssueSeverity::Error);
            return false;
        }
        QualType type = function->argumentTypes().front();
        auto* thisEntity = function->findEntity<Variable>("__this");
        lit.decorate(thisEntity, type);
        return true;
    }
    case String:
        lit.decorate(nullptr, sym.explRef(QualType::Const(sym.Str())));
        return true;

    case Char:
        lit.decorate(nullptr, sym.Byte());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return true;
    }
}

bool Context::analyzeImpl(ast::UnaryExpression& u) {
    if (!analyze(*u.operand())) {
        return false;
    }
    QualType operandType = u.operand()->type();
    QualType operandBaseType = stripQualifiers(operandType);
    switch (u.operation()) {
    case ast::UnaryOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryOperator::Negation:
        if (!isAny<IntType, FloatType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        dereference(u.operand(), sym);
        u.decorate(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        dereference(u.operand(), sym);
        u.decorate(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::LogicalNot:
        if (!isAny<BoolType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        dereference(u.operand(), sym);
        u.decorate(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::Increment:
        [[fallthrough]];
    case ast::UnaryOperator::Decrement: {
        if (!isAny<IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return false;
        }
        if (!convertToImplicitMutRef(u.operand(), sym, iss)) {
            return false;
        }
        switch (u.notation()) {
        case ast::UnaryOperatorNotation::Prefix: {
            QualType refType = sym.implRef(operandBaseType);
            u.decorate(u.operand()->entity(), refType);
            break;
        }
        case ast::UnaryOperatorNotation::Postfix: {
            u.decorate(nullptr, operandBaseType);
            break;
        }
        case ast::UnaryOperatorNotation::_count:
            SC_UNREACHABLE();
        }
        break;
    }
    case ast::UnaryOperator::_count:
        SC_UNREACHABLE();
    }
    u.setConstantValue(evalUnary(u.operation(), u.operand()->constantValue()));
    return true;
}

bool Context::analyzeImpl(ast::BinaryExpression& b) {
    bool success = true;
    success &= analyze(*b.lhs());
    success &= analyze(*b.rhs());
    if (!success) {
        return false;
    }
    QualType resultType = analyzeBinaryExpr(b);
    if (!resultType) {
        return false;
    }
    b.decorate(nullptr, resultType);
    b.setConstantValue(evalBinary(b.operation(),
                                  b.lhs()->constantValue(),
                                  b.rhs()->constantValue()));
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
            id.setConstantValue(clone(var.constantValue()));
            return true;
        },
        [&](ObjectType& type) {
            id.decorate(&type, nullptr);
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
        [&](PoisonEntity& poison) {
            id.decorate(&poison, nullptr);
            return false;
        },
        [&](Entity const& entity) -> bool {
            /// Other entities can not be referenced directly
            SC_UNREACHABLE();
        }
    }); // clang-format on
}

static Scope* findLookupTargetScope(ast::Expression& expr) {
    switch (expr.entityCategory()) {
    case EntityCategory::Value: {
        auto type = stripReference(expr.type());
        return const_cast<ObjectType*>(type.get());
    }
    case EntityCategory::Type:
        return cast<Scope*>(expr.entity());
    default:
        SC_UNREACHABLE();
    }
}

bool Context::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(*ma.object())) {
        return false;
    }
    Scope* lookupTargetScope = findLookupTargetScope(*ma.object());
    SC_ASSERT(lookupTargetScope,
              "analyze(ma.object()) should have failed if this is null");
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
        iss.push<InvalidNameLookup>(ma);
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
        const_cast<ObjectType*>(ma.object()->typeOrTypeEntity().get());
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
    auto funcRes =
        performOverloadResolution(overloadSet, std::array{ ma.object() }, true);
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
        convertExplicitly(call->argument(0),
                          makeRefImplicit(func->argumentType(0)),
                          iss);
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
    auto* referred = ref.referred();
    auto mutQual = ref.isMutable() ? Mutability::Mutable : Mutability::Const;

    switch (referred->entityCategory()) {
    case EntityCategory::Value: {
        if (!referred->isLValue() && !isRef(referred->type())) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return false;
        }
        auto* refType =
            sym.explRef(stripReference(referred->type()).toMutability(mutQual));
        if (!explicitConversionRank(referred, refType)) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return false;
        }
        ref.decorate(referred->entity(), refType, ValueCategory::RValue);
        return true;
    }
    case EntityCategory::Type: {
        auto* type = cast<ObjectType*>(referred->entity());
        auto* refType = sym.explRef(QualType(type, mutQual));
        ref.decorate(const_cast<ReferenceType*>(refType));
        return true;
    }
    default:
        /// Make an error class `InvalidReferenceExpression` and push that here
        SC_UNIMPLEMENTED();
    }
}

bool Context::analyzeImpl(ast::UniqueExpression& expr) { SC_UNIMPLEMENTED(); }

bool Context::analyzeImpl(ast::Conditional& c) {
    bool success = analyze(*c.condition());
    if (success) {
        convertImplicitly(c.condition(), sym.Bool(), iss);
    }
    success &= analyze(*c.thenExpr());
    success &= expectValue(*c.thenExpr());
    success &= analyze(*c.elseExpr());
    success &= expectValue(*c.elseExpr());
    if (!success) {
        return false;
    }
    QualType thenType = c.thenExpr()->type();
    QualType elseType = c.elseExpr()->type();
    QualType commonType = sema::commonType(sym, thenType, elseType);
    if (!commonType) {
        iss.push<BadOperandsForBinaryExpression>(c, thenType, elseType);
        return false;
    }
    success &= convertImplicitly(c.thenExpr(), commonType, iss);
    success &= convertImplicitly(c.elseExpr(), commonType, iss);
    SC_ASSERT(success,
              "Common type should not return a type if not both types are "
              "convertible to that type");
    c.decorate(nullptr, commonType);
    c.setConstantValue(evalConditional(c.condition()->constantValue(),
                                       c.thenExpr()->constantValue(),
                                       c.elseExpr()->constantValue()));
    return true;
}

bool Context::analyzeImpl(ast::Subscript& expr) {
    bool success = analyze(*expr.object());
    success &= expectValue(*expr.object());
    /// This must not be a `for each` loop, because the argument to the call to
    /// `analyze` may be deallocated and then the call to `expectValue()` would
    /// use freed memory without getting the argument again from `expr`
    for (size_t i = 0, end = expr.arguments().size(); i < end; ++i) {
        success &= analyze(*expr.argument(i));
        success &= expectValue(*expr.argument(i));
    }
    if (!success) {
        return false;
    }
    auto* arrayType =
        dyncast<ArrayType const*>(stripReference(expr.object()->type()).get());
    if (!arrayType) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    if (expr.arguments().size() != 1) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    auto& arg = *expr.arguments().front();
    if (!isa<IntType>(*arg.type())) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return false;
    }
    dereference(expr.object(), sym);
    dereference(expr.argument(0), sym);
    auto mutability = stripReference(expr.object()->type()).mutability();
    QualType elemType =
        sym.implRef(QualType(arrayType->elementType(), mutability));
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
    expr.decorate(nullptr, cast<ObjectType*>(expr.argument(0)->entity()));
    return true;
}

bool Context::analyzeImpl(ast::FunctionCall& fc) {
    bool success = analyze(*fc.object());
    bool isMemberCall = false;
    /// We analyze all the arguments
    for (size_t i = 0; i < fc.arguments().size(); ++i) {
        if (!analyze(*fc.argument(i))) {
            success = false;
            continue;
        }
        if (!expectValue(*fc.argument(i))) {
            success = false;
            continue;
        }
        if (!fc.argument(i)->type()) {
            iss.push<BadExpression>(*fc.argument(i), IssueSeverity::Error);
            success = false;
            continue;
        }
    }
    if (!success) {
        return false;
    }

    /// If our object is a member access expression, we rewrite the AST
    if (auto* ma = dyncast<ast::MemberAccess*>(fc.object());
        ma && ma->object()->isValue())
    {
        auto memberAccess = fc.extractObject<ast::MemberAccess>();
        auto memFunc = memberAccess->extractMember();
        auto objectArg = memberAccess->extractObject();
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
        QualType targetType = genExpr->type();
        auto* arg = fc.argument(0);
        fc.parent()->replaceChild(&fc, fc.extractArgument(0));
        return convertReinterpret(arg, targetType, iss);
    }

    /// if our object is a type, then we rewrite the AST so we end up with just
    /// a conversion node
    if (auto* targetType = dyncast<ObjectType const*>(fc.object()->entity())) {
        if (auto* structType = dyncast<StructureType const*>(targetType)) {
            auto args =
                fc.arguments() | ranges::views::transform([](auto* arg) {
                    return arg->extractFromParent();
                }) |
                ranges::to<utl::small_vector<UniquePtr<ast::Expression>>>;
            auto ctorCall = makeConstructorCall(structType,
                                                args,
                                                sym,
                                                iss,
                                                fc.sourceRange());
            if (!ctorCall) {
                return false;
            }
            dtorStack.push(ctorCall->object());
            fc.parent()->setChild(fc.indexInParent(), std::move(ctorCall));
            return true;
        }
        else {
            SC_ASSERT(fc.arguments().size() == 1, "For now...");
            auto* arg = fc.argument(0);
            fc.parent()->replaceChild(&fc, fc.extractArgument(0));
            return convertExplicitly(arg, targetType, iss);
        }
    }

    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast_or_null<OverloadSet*>(fc.object()->entity());
    if (!overloadSet) {
        iss.push<BadFunctionCall>(fc,
                                  nullptr,
                                  utl::vector<QualType>{},
                                  BadFunctionCall::Reason::ObjectNotCallable);
        return false;
    }

    /// Perform overload resolution
    auto result = performOverloadResolution(
        overloadSet,
        fc.arguments() | ranges::to<utl::small_vector<ast::Expression const*>>,
        isMemberCall);
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
    auto* first = list.elements().front();
    auto entityCat = first->entityCategory();
    switch (entityCat) {
    case EntityCategory::Value: {
        for (auto* expr: list.elements()) {
            if (!expr->isValue()) {
                iss.push<BadSymbolReference>(*expr, EntityCategory::Value);
                success = false;
                continue;
            }
            if (isExplRef(expr->type())) {
                /// TODO: Consider wether we allow storing references in arrays
                iss.push<BadExpression>(list, IssueSeverity::Error);
                success = false;
            }
        }
        if (!success) {
            return false;
        }
        auto elements = list.elements() |
                        ranges::to<utl::small_vector<ast::Expression const*>>;
        QualType commonType = sema::commonType(sym, elements);
        if (!commonType) {
            iss.push<InvalidListExpr>(list, InvalidListExpr::NoCommonType);
            return false;
        }
        for (auto* expr: list.elements()) {
            bool const succ = convertImplicitly(expr, commonType, iss);
            SC_ASSERT(succ,
                      "Conversion shall not fail if we have a common type");
        }
        auto* arrayType =
            sym.arrayType(commonType.get(), list.elements().size());
        list.decorate(nullptr, arrayType);
        return true;
    }
    case EntityCategory::Type: {
        auto* elementType = cast<ObjectType*>(first->entity());
        if (list.elements().size() != 1 && list.elements().size() != 2) {
            iss.push<InvalidListExpr>(
                list,
                InvalidListExpr::InvalidElemCountForArrayType);
            return false;
        }
        size_t count = ArrayType::DynamicCount;
        if (list.elements().size() == 2) {
            auto* countExpr = list.element(1);
            auto* countType = dyncast<IntType const*>(countExpr->type().get());
            auto* value =
                dyncast_or_null<IntValue const*>(countExpr->constantValue());
            if (!countType || !value ||
                (countType->isSigned() && value->value().negative()))
            {
                iss.push<BadExpression>(*countExpr, IssueSeverity::Error);
                return false;
            }
            count = value->value().to<size_t>();
        }
        auto* arrayType = sym.arrayType(elementType, count);
        list.decorate(const_cast<ArrayType*>(arrayType), nullptr);
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

QualType Context::analyzeBinaryExpr(ast::BinaryExpression& expr) {
    /// Handle comma operator separately
    if (expr.operation() == ast::BinaryOperator::Comma) {
        return expr.rhs()->type();
    }
    /// Also reference assignment is handled separately
    if (expr.operation() == ast::BinaryOperator::Assignment &&
        isExplRef(expr.rhs()->type()))
    {
        return analyzeReferenceAssignment(expr);
    }

    /// Determine common type
    QualType commonQualifiedType =
        sema::commonType(sym, expr.lhs()->type(), expr.rhs()->type());
    if (!commonQualifiedType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return nullptr;
    }
    QualType commonType = stripQualifiers(commonQualifiedType);

    auto resultType = getResultType(sym, commonType.get(), expr.operation());
    if (!resultType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return nullptr;
    }

    /// Handle value assignment seperately
    if (ast::isAssignment(expr.operation())) {
        QualType lhsType = expr.lhs()->type();
        /// Here we only look at assignment _through_ references
        /// That means LHS shall be an implicit reference
        bool success = true;
        success &= convertToImplicitMutRef(expr.lhs(), sym, iss);
        success &= convertImplicitly(expr.rhs(), stripQualifiers(lhsType), iss);
        return success ? sym.Void() : nullptr;
    }

    bool successfullConv = true;
    successfullConv &= convertImplicitly(expr.lhs(), commonType, iss);
    successfullConv &= convertImplicitly(expr.rhs(), commonType, iss);

    if (successfullConv) {
        return *resultType;
    }
    return nullptr;
}

QualType Context::analyzeReferenceAssignment(ast::BinaryExpression& expr) {
    SC_ASSERT(isExplRef(expr.rhs()->type()), "");
    if (!expr.lhs()->type().isMutable()) {
        iss.push<AssignmentToConst>(expr);
        return nullptr;
    }
    QualType explicitRefType = makeRefExplicit(expr.lhs()->type());
    bool success = true;
    success &= convertExplicitly(expr.lhs(), explicitRefType, iss);
    success &= convertExplicitly(expr.rhs(), explicitRefType, iss);
    return success ? sym.Void() : nullptr;
}

Entity* Context::lookup(ast::Identifier& id) {
    bool const restrictedLookup = performRestrictedNameLookup;
    performRestrictedNameLookup = false;
    auto* entity = restrictedLookup ?
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
