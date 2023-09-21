#include "Sema/Analysis/ExpressionAnalysis.h"

#include <optional>
#include <span>

#include <range/v3/algorithm.hpp>
#include <svm/Builtin.h>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/FunctionAnalysis.h"
#include "Sema/Analysis/Lifetime.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

namespace {

struct ExprContext {
    ExprContext(sema::AnalysisContext& ctx, DTorStack& dtorStack):
        dtorStack(&dtorStack),
        ctx(ctx),
        sym(ctx.symbolTable()),
        iss(ctx.issueHandler()) {}

    ast::Expression* analyze(ast::Expression*);

    /// Shorthand for `analyze() && expectValue()`
    ast::Expression* analyzeValue(ast::Expression*);

    ast::Expression* analyzeImpl(ast::Literal&);
    ast::Expression* analyzeImpl(ast::UnaryExpression&);
    ast::Expression* analyzeImpl(ast::BinaryExpression&);
    std::tuple<Object*, ValueCategory, QualType> analyzeBinaryExpr(
        ast::BinaryExpression&);
    ast::Expression* analyzeImpl(ast::Identifier&);
    ast::Expression* analyzeImpl(ast::MemberAccess&);
    ast::Expression* rewritePropertyCall(ast::MemberAccess&);
    ast::Expression* analyzeImpl(ast::DereferenceExpression&);
    ast::Expression* analyzeImpl(ast::AddressOfExpression&);
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

    /// Access the return type of a \p function
    /// If necessary this deduces the return type by calling `analyzeFunction()`
    /// \Returns `nullptr` if return type deduction failed
    Type const* getReturnType(Function* function);

    /// \Returns `true` if \p expr is a value. Otherwise submits an error and
    /// returns `false`
    bool expectValue(ast::Expression const* expr);

    /// \Returns `true` if \p expr is a type. Otherwise submits an error and
    /// returns `false`
    bool expectType(ast::Expression const* expr);

    DTorStack* dtorStack;
    sema::AnalysisContext& ctx;
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

ast::Expression* sema::analyzeExpression(ast::Expression* expr,
                                         DTorStack& dtorStack,
                                         AnalysisContext& ctx) {
    if (!expr) {
        return nullptr;
    }
    return ExprContext(ctx, dtorStack).analyze(expr);
}

Type const* sema::analyzeTypeExpression(ast::Expression* expr,
                                        AnalysisContext& ctx) {
    DTorStack dtorStack;
    expr = sema::analyzeExpression(expr, dtorStack, ctx);
    SC_ASSERT(dtorStack.empty(), "");
    if (!expr) {
        return nullptr;
    }
    if (!expr->isType()) {
        ctx.issueHandler().push<BadSymbolReference>(*expr,
                                                    EntityCategory::Type);
        return nullptr;
    }
    return cast<Type const*>(expr->entity());
}

ast::Expression* ExprContext::analyze(ast::Expression* expr) {
    if (expr->isDecorated()) {
        return expr;
    }
    expr = visit(*expr, [this](auto&& e) { return this->analyzeImpl(e); });
    SC_ASSERT(true, "");
    return expr;
}

ast::Expression* ExprContext::analyzeValue(ast::Expression* expr) {
    auto* result = analyze(expr);
    if (!result || !expectValue(result)) {
        return nullptr;
    }
    return result;
}

ast::Expression* ExprContext::analyzeImpl(ast::Literal& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        lit.decorateValue(sym.temporary(sym.S64()), RValue);
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ true));
        return &lit;

    case Boolean:
        lit.decorateValue(sym.temporary(sym.Bool()), RValue);
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return &lit;

    case FloatingPoint:
        lit.decorateValue(sym.temporary(sym.F64()), RValue);
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
        auto* thisEntity = function->findEntity<Variable>("__this");
        lit.decorateValue(thisEntity, LValue, thisEntity->getQualType());
        return &lit;
    }
    case String: {
        auto type = QualType::Const(sym.Str());
        lit.decorateValue(sym.temporary(type), LValue);
        return &lit;
    }

    case Char:
        lit.decorateValue(sym.temporary(sym.Byte()), RValue);
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
    QualType operandBaseType = operandType.toMut();
    switch (u.operation()) {
    case ast::UnaryOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryOperator::Negation:
        if (!isAny<IntType, FloatType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(operandBaseType), RValue);
        break;
    case ast::UnaryOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(operandBaseType), RValue);
        break;
    case ast::UnaryOperator::LogicalNot:
        if (!isAny<BoolType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(operandBaseType), RValue);
        break;
    case ast::UnaryOperator::Increment:
        [[fallthrough]];
    case ast::UnaryOperator::Decrement: {
        if (!isAny<IntType>(operandBaseType.get())) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        if (!u.operand()->isLValue() || !u.operand()->type().isMut()) {
            iss.push<BadOperandForUnaryExpression>(u, operandType);
            return nullptr;
        }
        switch (u.notation()) {
        case ast::UnaryOperatorNotation::Prefix: {
            u.decorateValue(u.operand()->entity(), LValue, operandType);
            break;
        }
        case ast::UnaryOperatorNotation::Postfix: {
            u.decorateValue(sym.temporary(operandBaseType), RValue);
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
    auto [object, valueCat, type] = analyzeBinaryExpr(b);
    if (!object) {
        return nullptr;
    }
    b.decorateValue(object, valueCat, type);
    b.setConstantValue(evalBinary(b.operation(),
                                  b.lhs()->constantValue(),
                                  b.rhs()->constantValue()));
    return &b;
}

/// FIXME: Do we really need optional pointer here?
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

std::tuple<Object*, ValueCategory, QualType> ExprContext::analyzeBinaryExpr(
    ast::BinaryExpression& expr) {
    /// Handle comma operator separately
    if (expr.operation() == ast::BinaryOperator::Comma) {
        return { expr.rhs()->object(),
                 expr.rhs()->valueCategory(),
                 expr.rhs()->type() };
    }

    /// Determine common type
    QualType commonType =
        sema::commonType(sym, expr.lhs()->type(), expr.rhs()->type());
    if (!commonType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return {};
    }
    auto resultType = getResultType(sym, commonType.get(), expr.operation());
    if (!resultType) {
        iss.push<BadOperandsForBinaryExpression>(expr,
                                                 expr.lhs()->type(),
                                                 expr.rhs()->type());
        return {};
    }

    /// Handle assignment seperately
    if (ast::isAssignment(expr.operation())) {
        QualType lhsType = expr.lhs()->type();
        /// Here we only look at assignment _through_ references
        /// That means LHS shall be an implicit reference
        if (!expr.lhs()->type().isMut()) {
            iss.push<BadOperandsForBinaryExpression>(expr,
                                                     expr.lhs()->type(),
                                                     expr.rhs()->type());
            return {};
        }
        bool success = true;
        success &=
            !!convert(Implicit, expr.rhs(), lhsType, RValue, *dtorStack, ctx);
        if (!success) {
            return {};
        }
        return { sym.temporary(sym.Void()), RValue, sym.Void() };
    }

    bool successfulConv = true;
    successfulConv &=
        !!convert(Implicit, expr.lhs(), commonType, RValue, *dtorStack, ctx);
    successfulConv &=
        !!convert(Implicit, expr.rhs(), commonType, RValue, *dtorStack, ctx);

    if (successfulConv) {
        return { sym.temporary(*resultType), RValue, *resultType };
    }
    return {};
}

ast::Expression* ExprContext::analyzeImpl(ast::Identifier& id) {
    Entity* entity = lookup(id);
    if (!entity) {
        return nullptr;
    }
    // clang-format off
    return SC_MATCH (*entity) {
        [&](Variable& var) {
            id.decorateValue(&var, LValue, var.getQualType());
            id.setConstantValue(clone(var.constantValue()));
            return &id;
        },
        [&](Property& prop) {
            id.decorateValue(&prop, prop.valueCategory());
            return &id;
        },
        [&](ObjectType& type) {
            id.decorateType(&type);
            return &id;
        },
        [&](OverloadSet& overloadSet) {
            id.decorateValue(&overloadSet, LValue);
            return &id;
        },
        [&](Generic& generic) {
            id.decorateValue(&generic, LValue);
            return &id;
        },
        [&](PoisonEntity& poison) {
            id.decorateValue(&poison, RValue);
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
        return const_cast<ObjectType*>(expr.type().get());
    }
    case EntityCategory::Type:
        return cast<Scope*>(expr.entity());
    default:
        SC_UNREACHABLE();
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::MemberAccess& ma) {
    /// For an expression of the kind `obj->member` we simply rewrite the AST so
    /// the expression becomes `(*obj).member`
    if (ma.operation() == ast::MemberAccessOperation::Pointer) {
        auto accessed = ma.accessed()->extractFromParent();
        auto deref =
            allocate<ast::DereferenceExpression>(std::move(accessed),
                                                 Mutability::Const,
                                                 accessed->sourceRange());
        ma.setAccessed(std::move(deref));
    }
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
        return nullptr;
    }
    if (isa<OverloadSet>(ma.member()->entity()) &&
        !isa<ast::FunctionCall>(ma.parent()))
    {
        return rewritePropertyCall(ma);
    }
    switch (ma.accessed()->entityCategory()) {
    case EntityCategory::Value: {
        if (!ma.member()->isValue()) {
            iss.push<InvalidNameLookup>(ma);
            return nullptr;
        }
        auto type = QualType(ma.member()->type().get(),
                             ma.accessed()->type().mutability());
        ma.decorateValue(sym.temporary(type), ma.member()->valueCategory());
        break;
    }
    case EntityCategory::Type:
        if (ma.member()->isValue()) {
            ma.decorateValue(ma.member()->entity(), LValue);
        }
        else if (ma.member()->isType()) {
            ma.decorateType(cast<Type*>(ma.member()->entity()));
        }
        break;
    case EntityCategory::Indeterminate:
        break;
    case EntityCategory::_count:
        SC_UNREACHABLE();
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
    auto* returnType = getReturnType(func);
    if (!returnType) {
        iss.push<BadFunctionCall>(
            ma,
            overloadSet,
            utl::small_vector<Type const*>{},
            BadFunctionCall::Reason::CantDeduceReturnType);
        return nullptr;
    }
    QualType type = getQualType(returnType);
    auto valueCat = isa<ReferenceType>(*returnType) ? LValue : RValue;
    auto* temp = sym.temporary(type);
    call->decorateCall(temp, valueCat, type, func);
    dtorStack->push(temp);
    bool const convSucc = convert(Explicit,
                                  call->argument(0),
                                  getQualType(func->argumentType(0)),
                                  refToLValue(func->argumentType(0)),
                                  *dtorStack,
                                  ctx);
    SC_ASSERT(convSucc,
              "If overload resolution succeeds conversion must not fail");

    /// Now `ma` goes out of scope
    auto* result = call.get();
    ma.parent()->replaceChild(&ma, std::move(call));
    return result;
}

ast::Expression* ExprContext::analyzeImpl(ast::DereferenceExpression& expr) {
    if (!analyze(expr.referred())) {
        return nullptr;
    }
    auto* pointer = expr.referred();
    switch (pointer->entityCategory()) {
    case EntityCategory::Value: {
        auto* ptrType = dyncast<PointerType const*>(pointer->type().get());
        if (!ptrType) {
            iss.push<BadExpression>(expr, IssueSeverity::Error);
            return nullptr;
        }
        expr.decorateValue(sym.temporary(ptrType->base()), LValue);
        return &expr;
    }
    case EntityCategory::Type: {
        auto* type = cast<ObjectType*>(pointer->entity());
        auto* ptrType = sym.pointer(QualType(type, expr.mutability()));
        expr.decorateType(const_cast<PointerType*>(ptrType));
        return &expr;
    }
    default:
        SC_UNIMPLEMENTED();
    }
}

static bool isConvertible(Mutability from, Mutability to) {
    using enum Mutability;
    return from == Mutable || to == Const;
}

ast::Expression* ExprContext::analyzeImpl(ast::AddressOfExpression& expr) {
    if (!analyze(expr.referred())) {
        return nullptr;
    }
    auto* referred = expr.referred();
    switch (referred->entityCategory()) {
    case EntityCategory::Value: {
        if (!referred->isLValue()) {
            iss.push<BadExpression>(expr, IssueSeverity::Error);
            return nullptr;
        }
        if (!isConvertible(referred->type().mutability(), expr.mutability())) {
            iss.push<BadExpression>(expr, IssueSeverity::Error);
            return nullptr;
        }
        auto referredType = QualType(referred->type().get(), expr.mutability());
        auto* ptrType = sym.pointer(referredType);
        expr.decorateValue(sym.temporary(ptrType), RValue);
        return &expr;
    }
    case EntityCategory::Type: {
        auto* type = cast<ObjectType*>(referred->entity());
        auto* refType = sym.reference(QualType(type, expr.mutability()));
        expr.decorateType(const_cast<ReferenceType*>(refType));
        return &expr;
    }
    default:
        /// Make an error class `InvalidReferenceExpression` and push that here
        SC_UNIMPLEMENTED();
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::Conditional& c) {
    if (analyzeValue(c.condition())) {
        convert(Implicit, c.condition(), sym.Bool(), RValue, *dtorStack, ctx);
    }
    auto* commonDtors = dtorStack;
    auto* lhsDtors = &c.branchDTorStack(0);
    auto* rhsDtors = &c.branchDTorStack(1);

    bool success = true;
    dtorStack = lhsDtors;
    success &= !!analyzeValue(c.thenExpr());
    dtorStack = rhsDtors;
    success &= !!analyzeValue(c.elseExpr());
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
    auto commonValueCat = sema::commonValueCat(c.thenExpr()->valueCategory(),
                                               c.elseExpr()->valueCategory());
    success &= !!convert(Implicit,
                         c.thenExpr(),
                         commonType,
                         commonValueCat,
                         c.branchDTorStack(0),
                         ctx);
    success &= !!convert(Implicit,
                         c.elseExpr(),
                         commonType,
                         commonValueCat,
                         c.branchDTorStack(1),
                         ctx);
    SC_ASSERT(success,
              "Common type should not return a type if not both types are "
              "convertible to that type");
    c.decorateValue(sym.temporary(commonType), commonValueCat);
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
    convert(Implicit, expr.argument(0), sym.S64(), RValue, *dtorStack, ctx);
    auto mutability = expr.callee()->type().mutability();
    auto elemType = QualType(arrayType->elementType(), mutability);
    expr.decorateValue(sym.temporary(elemType), expr.callee()->valueCategory());
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::SubscriptSlice& expr) {
    auto* arrayType = analyzeSubscriptCommon(expr);
    if (!arrayType) {
        return nullptr;
    }
    convert(Implicit, expr.lower(), sym.S64(), RValue, *dtorStack, ctx);
    convert(Implicit, expr.upper(), sym.S64(), RValue, *dtorStack, ctx);
    auto dynArrayType = sym.arrayType(arrayType->elementType());
    expr.decorateValue(sym.temporary(dynArrayType),
                       expr.callee()->valueCategory());
    return &expr;
}

ArrayType const* ExprContext::analyzeSubscriptCommon(ast::CallLike& expr) {
    bool success = !!analyzeValue(expr.callee());
    for (auto* arg: expr.arguments()) {
        success &= !!analyzeValue(arg);
    }
    if (!success) {
        return nullptr;
    }
    auto* accessedType = expr.callee()->type().get();
    auto* arrayType = dyncast<ArrayType const*>(accessedType);
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
    auto* resultType = cast<Type const*>(expr.argument(0)->entity());
    expr.decorateValue(sym.temporary(getQualType(resultType)),
                       refToLValue(resultType));
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

    /// If our object is a generic expression, we assert that it is a
    /// `reinterpret` expression and rewrite the AST
    if (auto* genExpr = dyncast<ast::GenericExpression*>(fc.callee())) {
        SC_ASSERT(genExpr->callee()->entity()->name() == "reinterpret", "");
        SC_ASSERT(fc.arguments().size() == 1, "");
        auto* arg = fc.argument(0);
        auto* converted = convert(Reinterpret,
                                  arg,
                                  genExpr->type(),
                                  genExpr->valueCategory(),
                                  *dtorStack,
                                  ctx);
        fc.parent()->replaceChild(&fc, fc.extractArgument(0));
        return converted;
    }

    /// if our object is a type, then we rewrite the AST so we end up with just
    /// a conversion node
    if (auto* targetType = dyncast<ObjectType const*>(fc.callee()->entity())) {
        auto args = fc.arguments() | ranges::views::transform([](auto* arg) {
                        return arg->extractFromParent();
                    }) |
                    ToSmallVector<>;
        auto ctorCall = makePseudoConstructorCall(targetType,
                                                  nullptr,
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

    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast_or_null<OverloadSet*>(fc.callee()->entity());
    if (!overloadSet) {
        iss.push<BadFunctionCall>(fc,
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
    auto* returnType = getReturnType(function);
    if (!returnType) {
        iss.push<BadFunctionCall>(
            fc,
            overloadSet,
            fc.arguments() | ranges::views::transform([](auto* arg) {
                return arg->type().get();
            }) | ToSmallVector<>,
            BadFunctionCall::Reason::CantDeduceReturnType);
        return nullptr;
    }
    QualType type = getQualType(returnType);
    auto valueCat = isa<ReferenceType>(*returnType) ? LValue : RValue;
    fc.decorateCall(sym.temporary(type), valueCat, type, function);
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
        list.decorateValue(sym.temporary(nullptr), RValue);
        return nullptr;
    }
    auto* first = list.elements().front();
    auto entityCat = first->entityCategory();
    switch (entityCat) {
    case EntityCategory::Value: {
        for (auto* expr: list.elements()) {
            success &= expectValue(expr);
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
                convert(Implicit, expr, commonType, RValue, *dtorStack, ctx);
            SC_ASSERT(succ, "Conversion failed despite common type");
        }
        auto* arrayType =
            sym.arrayType(commonType.get(), list.elements().size());
        list.decorateValue(sym.temporary(arrayType), RValue);
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
        list.decorateType(const_cast<ArrayType*>(arrayType));
        return &list;
    }
    default:
        SC_UNREACHABLE();
    }
}

Entity* ExprContext::lookup(ast::Identifier& id) {
    bool const restrictedLookup = performRestrictedNameLookup;
    performRestrictedNameLookup = false;
    auto* entity = restrictedLookup ?
                       sym.currentScope().findEntity(id.value()) :
                       sym.lookup(id.value());
    if (!entity) {
        iss.push<UseOfUndeclaredIdentifier>(id, sym.currentScope());
    }
    return entity;
}

Type const* ExprContext::getReturnType(Function* function) {
    if (function->returnType()) {
        return function->returnType();
    }
    analyzeFunction(ctx, cast<ast::FunctionDefinition*>(function->astNode()));
    if (function->returnType()) {
        return function->returnType();
    }
    /// If we get here we failed to deduce the return type because of cycles in
    /// the call graph
    return nullptr;
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
