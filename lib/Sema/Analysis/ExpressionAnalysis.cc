#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Context.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct ExprContext {
    ExprContext(sema::Context& ctx, DTorStack& dtorStack):
        dtorStack(&dtorStack),
        ctx(ctx),
        sym(ctx.symbolTable()),
        iss(ctx.issueHandler()) {}

    ast::Expression* analyze(ast::Expression*);

    ast::Expression* analyzeImpl(ast::Literal&);
    ast::Expression* analyzeImpl(ast::UnaryExpression&);
    ast::Expression* analyzeImpl(ast::BinaryExpression&);

    ast::Expression* analyzeImpl(ast::Identifier&);
    ast::Expression* analyzeImpl(ast::MemberAccess&);
    ast::Expression* uniformFunctionCall(ast::MemberAccess&);
    ast::Expression* rewritePropertyCall(ast::MemberAccess&);
    ast::Expression* analyzeImpl(ast::ReferenceExpression&);
    ast::Expression* analyzeImpl(ast::UniqueExpression&);
    ast::Expression* analyzeImpl(ast::Conditional&);
    ast::Expression* analyzeImpl(ast::FunctionCall&);
    ast::Expression* rewriteMemberCall(ast::FunctionCall&);
    ast::Expression* analyzeImpl(ast::Subscript&);
    ast::Expression* analyzeImpl(ast::SubscriptSlice&);
    ArrayType const* analyzeSubscriptCommon(ast::CallLike&);
    ast::Expression* analyzeImpl(ast::GenericExpression&);
    ast::Expression* analyzeImpl(ast::ListExpression&);

    ast::Expression* analyzeImpl(ast::ASTNode&) { SC_UNREACHABLE(); }

    Entity* lookup(ast::Identifier& id);

    /// \Returns `true` if \p expr is a value. Otherwise submits an error and
    /// returns `false`
    bool expectValue(ast::Expression const* expr);

    /// \Returns `true` if \p expr is a type. Otherwise submits an error and
    /// returns `false`
    bool expectType(ast::Expression const* expr);

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

    DTorStack* dtorStack;
    sema::Context& ctx;
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
                             Context& ctx) {
    return !!ExprContext(ctx, dtorStack).analyze(&expr);
}

ast::Expression* ExprContext::analyze(ast::Expression* expr) {
    if (expr->isDecorated()) {
        return expr;
    }
    expr = visit(*expr, [this](auto&& e) { return this->analyzeImpl(e); });
    SC_ASSERT(true, "");
    return expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::Literal& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        lit.decorateExpr(nullptr, sym.S64());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ true));
        return &lit;

    case Boolean:
        lit.decorateExpr(nullptr, sym.Bool());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return &lit;

    case FloatingPoint:
        lit.decorateExpr(nullptr, sym.F64());
        lit.setConstantValue(allocate<FloatValue>(lit.value<APFloat>()));
        return &lit;

    case This: {
        auto* scope = &sym.currentScope();
        while (!isa<Function>(scope)) {
            scope = scope->parent();
        }
        auto* function = cast<Function*>(scope);
        if (!function->isMember()) {
            iss.push<BadExpression>(lit, IssueSeverity::Error);
            return nullptr;
        }
        QualType type = function->argumentTypes().front();
        auto* thisEntity = function->findEntity<Variable>("__this");
        lit.decorateExpr(thisEntity, type);
        return &lit;
    }
    case String:
        lit.decorateExpr(nullptr, sym.explRef(QualType::Const(sym.Str())));
        return &lit;

    case Char:
        lit.decorateExpr(nullptr, sym.Byte());
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return &lit;
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::UnaryExpression& u) {
    if (!analyze(u.operand())) {
        return nullptr;
    }
    QualType operandType = u.operand()->type();
    QualType operandBaseType = stripQualifiers(operandType);
    switch (u.operation()) {
    case ast::UnaryOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryOperator::Negation:
        if (!isAny<IntType, FloatType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        dereference(u.operand(), ctx);
        u.decorateExpr(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        dereference(u.operand(), ctx);
        u.decorateExpr(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::LogicalNot:
        if (!isAny<BoolType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        dereference(u.operand(), ctx);
        u.decorateExpr(nullptr, operandBaseType);
        break;
    case ast::UnaryOperator::Increment:
        [[fallthrough]];
    case ast::UnaryOperator::Decrement: {
        if (!isAny<IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        if (!convertToImplicitMutRef(u.operand(), ctx)) {
            return nullptr;
        }
        switch (u.notation()) {
        case ast::UnaryOperatorNotation::Prefix: {
            QualType refType = sym.implRef(operandBaseType);
            u.decorateExpr(u.operand()->entity(), refType);
            break;
        }
        case ast::UnaryOperatorNotation::Postfix: {
            u.decorateExpr(nullptr, operandBaseType);
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
    return &u;
}

ast::Expression* ExprContext::analyzeImpl(ast::BinaryExpression& b) {
    bool success = true;
    success &= !!analyze(b.lhs());
    success &= !!analyze(b.rhs());
    if (!success) {
        return nullptr;
    }
    QualType resultType = analyzeBinaryExpr(b);
    if (!resultType) {
        return nullptr;
    }
    b.decorateExpr(nullptr, resultType);
    b.setConstantValue(evalBinary(b.operation(),
                                  b.lhs()->constantValue(),
                                  b.rhs()->constantValue()));
    return &b;
}

ast::Expression* ExprContext::analyzeImpl(ast::Identifier& id) {
    Entity* entity = lookup(id);
    if (!entity) {
        return nullptr;
    }
    // clang-format off
    return SC_MATCH (*entity) {
        [&](Variable& var) {
            id.decorateExpr(&var, makeRefImplicit(var.type()));
            id.setConstantValue(clone(var.constantValue()));
            return &id;
        },
        [&](ObjectType& type) {
            id.decorateExpr(&type);
            return &id;
        },
        [&](OverloadSet& overloadSet) {
            id.decorateExpr(&overloadSet);
            return &id;
        },
        [&](Generic& generic) {
            id.decorateExpr(&generic);
            return &id;
        },
        [&](PoisonEntity& poison) {
            id.decorateExpr(&poison);
            return nullptr;
        },
        [&](Entity const& entity) -> ast::Expression* {
            /// Other entities can not be referenced directly
            SC_UNREACHABLE();
        }
    }; // clang-format on
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

ast::Expression* ExprContext::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(ma.accessed())) {
        return nullptr;
    }
    Scope* lookupTargetScope = findLookupTargetScope(*ma.accessed());
    SC_ASSERT(lookupTargetScope,
              "analyze(ma.accessed()) should have failed if this is null");
    bool success = sym.withScopeCurrent(lookupTargetScope, [&] {
        /// We restrict name lookup to the
        /// current scope. This flag will be unset by the identifier case.
        performRestrictedNameLookup = true;
        return analyze(ma.member());
    });
    if (!success) {
        success = uniformFunctionCall(ma);
    }
    if (!success) {
        return nullptr;
    }
    if (isa<OverloadSet>(ma.member()->entity()) &&
        !isa<ast::FunctionCall>(ma.parent()))
    {
        return rewritePropertyCall(ma);
    }
    if (ma.accessed()->isValue() && !ma.member()->isValue()) {
        iss.push<InvalidNameLookup>(ma);
        return nullptr;
    }
    ma.decorateExpr(ma.member()->entity(),
                    ma.member()->type(),
                    ma.accessed()->valueCategory(),
                    ma.member()->entityCategory());
    /// Dereference the object if its a value
    if (ma.accessed()->isValue()) {
        dereference(ma.accessed(), ctx);
    }
    return &ma;
}

ast::Expression* ExprContext::uniformFunctionCall(ast::MemberAccess& ma) {
    Scope* lookupTargetScope =
        const_cast<ObjectType*>(ma.accessed()->typeOrTypeEntity().get());
    /// If we don't find a member and our object is a value, we will look
    /// for a function in the scope of the type of our object
    if (!ma.accessed()->isValue()) {
        /// Need to push the error here because the `Identifier` case does
        /// not push an error in member access lookup
        iss.push<UseOfUndeclaredIdentifier>(*ma.member(), *lookupTargetScope);
        return nullptr;
    }
    bool success = !!sym.withScopeCurrent(lookupTargetScope->parent(),
                                          [&] { return analyze(ma.member()); });
    if (!success) {
        return nullptr;
    }
    auto* overloadSet = dyncast<OverloadSet*>(ma.member()->entity());
    if (!overloadSet) {
        iss.push<UseOfUndeclaredIdentifier>(*ma.member(), *lookupTargetScope);
        return nullptr;
    }
    return &ma;
}

ast::Expression* ExprContext::rewritePropertyCall(ast::MemberAccess& ma) {
    SC_ASSERT(!isa<ast::FunctionCall>(ma.parent()), "Precondition");
    /// We reference an overload set, so since our parent is not a call
    /// expression we rewrite the AST here
    auto* overloadSet = cast<OverloadSet*>(ma.member()->entity());
    auto funcRes = performOverloadResolution(overloadSet,
                                             std::array{ ma.accessed() },
                                             true);
    if (funcRes.error) {
        funcRes.error->setSourceRange(ma.sourceRange());
        iss.push(std::move(funcRes.error));
        return nullptr;
    }
    auto* func = funcRes.function;
    utl::small_vector<UniquePtr<ast::Expression>> args;
    args.push_back(ma.extractAccessed());
    auto call = allocate<ast::FunctionCall>(ma.extractMember(),
                                            std::move(args),
                                            ma.sourceRange());
    QualType type = makeRefImplicit(func->returnType());
    auto* temp = &sym.addTemporary(type);
    call->decorateCall(temp, type, func);
    dtorStack->push(temp);
    bool const convSucc =
        convertExplicitly(call->argument(0),
                          makeRefImplicit(func->argumentType(0)),
                          *dtorStack,
                          ctx);
    SC_ASSERT(convSucc,
              "If overload resolution succeeds conversion must not fail");

    /// Now `ma` goes out of scope
    auto* result = call.get();
    ma.parent()->replaceChild(&ma, std::move(call));
    return result;
}

ast::Expression* ExprContext::analyzeImpl(ast::ReferenceExpression& ref) {
    if (!analyze(ref.referred())) {
        return nullptr;
    }
    auto* referred = ref.referred();
    switch (referred->entityCategory()) {
    case EntityCategory::Value: {
        if (!referred->isLValue() && !isRef(referred->type())) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return nullptr;
        }
        /// We only allow `mut` specifier on reference type declarations.
        /// For values the mutability is derived from the referred value
        if (ref.isMutable()) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return nullptr;
        }
        auto* refType = sym.explRef(stripReference(referred->type()));
        if (!computeConversion(ConversionKind::Explicit, referred, refType)) {
            iss.push<BadExpression>(ref, IssueSeverity::Error);
            return nullptr;
        }
        ref.decorateExpr(referred->entity(), refType, ValueCategory::RValue);
        return &ref;
    }
    case EntityCategory::Type: {
        auto* type = cast<ObjectType*>(referred->entity());
        auto* refType = sym.explRef(QualType(type, ref.mutability()));
        ref.decorateExpr(const_cast<ReferenceType*>(refType));
        return &ref;
    }
    default:
        /// Make an error class `InvalidReferenceExpression` and push that here
        SC_UNIMPLEMENTED();
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::UniqueExpression& expr) {
    SC_UNIMPLEMENTED();
}

ast::Expression* ExprContext::analyzeImpl(ast::Conditional& c) {
    bool success = !!analyze(c.condition());
    if (success) {
        convertImplicitly(c.condition(), sym.Bool(), *dtorStack, ctx);
    }
    auto* commonDtors = dtorStack;
    auto* lhsDtors = &c.branchDTorStack(0);
    auto* rhsDtors = &c.branchDTorStack(1);

    dtorStack = lhsDtors;
    success &= !!analyze(c.thenExpr());
    success &= expectValue(c.thenExpr());
    dtorStack = rhsDtors;
    success &= !!analyze(c.elseExpr());
    success &= expectValue(c.elseExpr());
    dtorStack = commonDtors;
    if (!success) {
        return nullptr;
    }
    QualType thenType = c.thenExpr()->type();
    QualType elseType = c.elseExpr()->type();
    QualType commonType = sema::commonType(sym, thenType, elseType);
    if (!commonType) {
        iss.push<BadOperandsForBinaryExpression>(c, thenType, elseType);
        return nullptr;
    }
    success &= !!convertImplicitly(c.thenExpr(),
                                   commonType,
                                   c.branchDTorStack(0),
                                   ctx);
    success &= !!convertImplicitly(c.elseExpr(),
                                   commonType,
                                   c.branchDTorStack(1),
                                   ctx);
    SC_ASSERT(success,
              "Common type should not return a type if not both types are "
              "convertible to that type");

    c.decorateExpr(nullptr, commonType);
    c.setConstantValue(evalConditional(c.condition()->constantValue(),
                                       c.thenExpr()->constantValue(),
                                       c.elseExpr()->constantValue()));
    return &c;
}

ast::Expression* ExprContext::analyzeImpl(ast::Subscript& expr) {
    auto* arrayType = analyzeSubscriptCommon(expr);
    if (!arrayType) {
        return nullptr;
    }
    if (expr.arguments().size() != 1) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return nullptr;
    }
    auto& arg = *expr.arguments().front();
    if (!isa<IntType>(*arg.type())) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return nullptr;
    }
    dereference(expr.callee(), ctx);
    dereference(expr.argument(0), ctx);
    auto mutability = stripReference(expr.callee()->type()).mutability();
    QualType elemType =
        sym.implRef(QualType(arrayType->elementType(), mutability));
    expr.decorateExpr(nullptr, elemType);
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::SubscriptSlice& expr) {
    auto* arrayType = analyzeSubscriptCommon(expr);
    if (!arrayType) {
        return nullptr;
    }
    auto& lower = *expr.lower();
    if (!isa<IntType>(*lower.type())) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return nullptr;
    }
    auto& upper = *expr.upper();
    if (!isa<IntType>(*upper.type())) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return nullptr;
    }
    dereference(expr.callee(), ctx);
    dereference(&lower, ctx);
    dereference(&upper, ctx);
    auto dynArrayType = sym.arrayType(arrayType->elementType());
    QualType arrayRefType = sym.implRef(dynArrayType);
    expr.decorateExpr(nullptr, arrayRefType);
    return &expr;
}

ArrayType const* ExprContext::analyzeSubscriptCommon(ast::CallLike& expr) {
    bool success = !!analyze(expr.callee());
    success &= expectValue(expr.callee());
    /// This must not be a `for each` loop, because the argument to the call to
    /// `analyze` may be deallocated and then the call to `expectValue()` would
    /// use freed memory without getting the argument again from `expr`
    for (size_t i = 0, end = expr.arguments().size(); i < end; ++i) {
        success &= !!analyze(expr.argument(i));
        success &= expectValue(expr.argument(i));
    }
    if (!success) {
        return nullptr;
    }
    auto* arrayType =
        dyncast<ArrayType const*>(stripReference(expr.callee()->type()).get());
    if (!arrayType) {
        iss.push<BadExpression>(expr, IssueSeverity::Error);
        return nullptr;
    }
    return arrayType;
}

ast::Expression* ExprContext::analyzeImpl(ast::GenericExpression& expr) {
    bool success = analyze(expr.callee());
    for (auto* arg: expr.arguments()) {
        success &= !!analyze(arg);
    }
    if (!success) {
        return nullptr;
    }
    SC_ASSERT(cast<ast::Identifier*>(expr.callee())->value() == "reinterpret",
              "For now");
    SC_ASSERT(expr.arguments().size() == 1, "For now");
    SC_ASSERT(expr.argument(0)->isType(), "For now");
    expr.decorateExpr(nullptr, cast<ObjectType*>(expr.argument(0)->entity()));
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::FunctionCall& fc) {
    bool success = analyze(fc.callee());
    bool isMemberCall = false;
    /// We analyze all the arguments
    for (size_t i = 0; i < fc.arguments().size(); ++i) {
        if (!analyze(fc.argument(i))) {
            success = false;
            continue;
        }
        if (!expectValue(fc.argument(i))) {
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
        return nullptr;
    }

    /// If our object is a member access into a value, we rewrite the AST
    if (auto* ma = dyncast<ast::MemberAccess*>(fc.callee());
        ma && ma->accessed()->isValue())
    {
        auto memberAccess = fc.extractCallee<ast::MemberAccess>();
        auto memFunc = memberAccess->extractMember();
        auto objectArg = memberAccess->extractAccessed();
        fc.insertArgument(0, std::move(objectArg));
        fc.setCallee(std::move(memFunc));
        isMemberCall = true;
        /// Member access object `object` goes out of scope and is destroyed
    }

    /// If our object is a generic expression, we assert that is a `reinterpret`
    /// expression and rewrite the AST
    if (auto* genExpr = dyncast<ast::GenericExpression*>(fc.callee())) {
        SC_ASSERT(genExpr->callee()->entity()->name() == "reinterpret", "");
        SC_ASSERT(fc.arguments().size() == 1, "");
        QualType targetType = genExpr->type();
        auto* arg = fc.argument(0);
        fc.parent()->replaceChild(&fc, fc.extractArgument(0));
        return convertReinterpret(arg, targetType, ctx);
    }

    /// if our object is a type, then we rewrite the AST so we end up with just
    /// a conversion node
    if (auto* targetType = dyncast<ObjectType const*>(fc.callee()->entity())) {
        if (auto* structType = dyncast<StructureType const*>(targetType)) {
            auto args = fc.arguments() |
                        ranges::views::transform([](auto* arg) {
                            return arg->extractFromParent();
                        }) |
                        ToSmallVector<>;
            auto ctorCall = makeConstructorCall(structType,
                                                std::move(args),
                                                *dtorStack,
                                                ctx,
                                                fc.sourceRange());
            if (!ctorCall) {
                return nullptr;
            }
            dtorStack->push(ctorCall->object());
            auto* result = ctorCall.get();
            fc.parent()->setChild(fc.indexInParent(), std::move(ctorCall));
            return result;
        }
        else {
            SC_ASSERT(fc.arguments().size() == 1, "For now...");
            auto* arg = fc.argument(0);
            fc.parent()->replaceChild(&fc, fc.extractArgument(0));
            return convertExplicitly(arg, targetType, *dtorStack, ctx);
        }
    }

    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast_or_null<OverloadSet*>(fc.callee()->entity());
    if (!overloadSet) {
        iss.push<BadFunctionCall>(fc,
                                  nullptr,
                                  utl::vector<QualType>{},
                                  BadFunctionCall::Reason::ObjectNotCallable);
        return nullptr;
    }

    /// Perform overload resolution
    auto result = performOverloadResolution(overloadSet,
                                            fc.arguments() | ToSmallVector<>,
                                            isMemberCall);
    if (result.error) {
        result.error->setSourceRange(fc.sourceRange());
        iss.push(std::move(result.error));
        return nullptr;
    }
    auto* function = result.function;
    QualType type = makeRefImplicit(function->returnType());
    fc.decorateCall(&sym.addTemporary(type), type, function);
    convertArguments(fc, result, *dtorStack, ctx);
    dtorStack->push(fc.object());
    return &fc;
}

ast::Expression* ExprContext::analyzeImpl(ast::ListExpression& list) {
    bool success = true;
    for (auto* expr: list.elements()) {
        success &= !!analyze(expr);
    }
    if (!success) {
        return nullptr;
    }
    if (list.elements().empty()) {
        list.decorateExpr(nullptr,
                          nullptr,
                          std::nullopt,
                          EntityCategory::Indeterminate);
        return nullptr;
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
                /// TODO: Consider whether we allow storing references in arrays
                iss.push<BadExpression>(list, IssueSeverity::Error);
                success = false;
            }
        }
        if (!success) {
            return nullptr;
        }
        auto elements = list.elements() | ToSmallVector<>;
        QualType commonType = sema::commonType(sym, elements);
        if (!commonType || isa<VoidType>(*commonType)) {
            iss.push<InvalidListExpr>(list, InvalidListExpr::NoCommonType);
            return nullptr;
        }
        for (auto* expr: list.elements()) {
            bool const succ =
                convertImplicitly(expr, commonType, *dtorStack, ctx);
            SC_ASSERT(succ,
                      "Conversion shall not fail if we have a common type");
        }
        auto* arrayType =
            sym.arrayType(commonType.get(), list.elements().size());
        list.decorateExpr(nullptr, arrayType);
        return &list;
    }
    case EntityCategory::Type: {
        auto* elementType = cast<ObjectType*>(first->entity());
        if (list.elements().size() != 1 && list.elements().size() != 2) {
            iss.push<InvalidListExpr>(
                list,
                InvalidListExpr::InvalidElemCountForArrayType);
            return nullptr;
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
                return nullptr;
            }
            count = value->value().to<size_t>();
        }
        auto* arrayType = sym.arrayType(elementType, count);
        list.decorateExpr(const_cast<ArrayType*>(arrayType));
        return &list;
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

QualType ExprContext::analyzeBinaryExpr(ast::BinaryExpression& expr) {
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
        success &= !!convertToImplicitMutRef(expr.lhs(), ctx);
        success &= !!convertImplicitly(expr.rhs(),
                                       stripQualifiers(lhsType),
                                       *dtorStack,
                                       ctx);
        return success ? sym.Void() : nullptr;
    }

    bool successfullConv = true;
    successfullConv &=
        !!convertImplicitly(expr.lhs(), commonType, *dtorStack, ctx);
    successfullConv &=
        !!convertImplicitly(expr.rhs(), commonType, *dtorStack, ctx);

    if (successfullConv) {
        return *resultType;
    }
    return nullptr;
}

QualType ExprContext::analyzeReferenceAssignment(ast::BinaryExpression& expr) {
    SC_ASSERT(isExplRef(expr.rhs()->type()), "");
    if (!expr.lhs()->type().isMutable()) {
        iss.push<AssignmentToConst>(expr);
        return nullptr;
    }
    QualType explicitRefType = makeRefExplicit(expr.lhs()->type());
    bool success = true;
    success &=
        !!convertExplicitly(expr.lhs(), explicitRefType, *dtorStack, ctx);
    success &=
        !!convertExplicitly(expr.rhs(), explicitRefType, *dtorStack, ctx);
    return success ? sym.Void() : nullptr;
}

Entity* ExprContext::lookup(ast::Identifier& id) {
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

bool ExprContext::expectValue(ast::Expression const* expr) {
    if (!expr->isDecorated()) {
        return false;
    }
    if (!expr->isValue()) {
        iss.push<BadSymbolReference>(*expr, EntityCategory::Value);
        return false;
    }
    return true;
}

bool ExprContext::expectType(ast::Expression const* expr) {
    if (!expr->isType()) {
        iss.push<BadSymbolReference>(*expr, EntityCategory::Type);
        return false;
    }
    return true;
}
