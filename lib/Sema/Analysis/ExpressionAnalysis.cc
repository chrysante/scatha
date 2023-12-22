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
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;
using enum BadExpr::Reason;

namespace {

struct ExprContext {
    DtorStack* dtorStack;
    sema::AnalysisContext& ctx;
    SymbolTable& sym;

    ExprContext(sema::AnalysisContext& ctx, DtorStack* dtorStack):
        dtorStack(dtorStack), ctx(ctx), sym(ctx.symbolTable()) {}

    ast::Expression* analyze(ast::Expression*);

    /// Shorthand for `analyze() && expectValue()`
    ast::Expression* analyzeValue(ast::Expression*);

    ///
    Type const* analyzeType(ast::Expression* expr);

    ast::Expression* analyzeImpl(ast::Literal&);
    ast::Expression* analyzeImpl(ast::UnaryExpression&);
    ast::Expression* analyzeImpl(ast::BinaryExpression&);
    ast::Expression* analyzeImpl(ast::Identifier&);
    ast::Expression* analyzeImpl(ast::MemberAccess&);
    ast::Expression* analyzeImpl(ast::DereferenceExpression&);
    ast::Expression* analyzeImpl(ast::AddressOfExpression&);
    ast::Expression* analyzeImpl(ast::Conditional&);
    ast::Expression* analyzeImpl(ast::MoveExpr&);
    ast::Expression* analyzeImpl(ast::UniqueExpr&);
    ast::Expression* analyzeImpl(ast::FunctionCall&);
    ast::Expression* rewriteMemberCall(ast::FunctionCall&);
    ast::Expression* analyzeImpl(ast::Subscript&);
    ast::Expression* analyzeImpl(ast::SubscriptSlice&);
    ArrayType const* analyzeSubscriptCommon(ast::CallLike&);
    ast::Expression* analyzeImpl(ast::GenericExpression&);
    ast::Expression* analyzeImpl(ast::ListExpression&);
    ast::Expression* analyzeImpl(ast::NonTrivAssignExpr&);
    ast::Expression* analyzeImpl(ast::Conversion&);
    ast::Expression* analyzeImpl(ast::ConstructExpr&);
    ast::Expression* analyzeImpl(ast::ASTNode&) { SC_UNREACHABLE(); }

    /// If \p expr is a pointer, inserts a `DereferenceExpression` as its
    /// parent. Expects \p expr to be analyzed. This is used to implicitly
    /// dereference pointers in member access and subscript expressions
    void dereferencePointer(ast::Expression* expr);

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
};

} // namespace

template <typename... Args, typename T>
static bool isAny(T const& t) {
    return (isa<Args>(t) || ...);
}

ast::Expression* sema::analyzeExpression(ast::Expression* expr,
                                         DtorStack& dtorStack,
                                         AnalysisContext& ctx) {
    return ExprContext(ctx, &dtorStack).analyze(expr);
}

ast::Expression* sema::analyzeValueExpr(ast::Expression* expr,
                                        DtorStack& dtorStack,
                                        AnalysisContext& ctx) {
    return ExprContext(ctx, &dtorStack).analyzeValue(expr);
}

Type const* sema::analyzeTypeExpr(ast::Expression* expr, AnalysisContext& ctx) {
    return ExprContext(ctx, nullptr).analyzeType(expr);
}

ast::Expression* ExprContext::analyze(ast::Expression* expr) {
    if (!expr) {
        return nullptr;
    }
    /// This check is helpful for some AST rewrites. This way we can just call
    /// `analyze()` on the inserted parent expression and already analyzed
    /// children will be ignored.
    if (expr->isDecorated()) {
        return expr;
    }
    return visit(*expr, [this](auto&& e) { return this->analyzeImpl(e); });
}

ast::Expression* ExprContext::analyzeValue(ast::Expression* expr) {
    auto* result = analyze(expr);
    if (!expectValue(result)) {
        return nullptr;
    }
    return result;
}

Type const* ExprContext::analyzeType(ast::Expression* expr) {
    auto* stashed = dtorStack;
    DtorStack tmpDtorStack;
    dtorStack = &tmpDtorStack;
    expr = analyze(expr);
    SC_ASSERT(dtorStack->empty(),
              "Type expression should not trigger destructor calls");
    dtorStack = stashed;
    if (!expectType(expr)) {
        return nullptr;
    }
    return cast<Type const*>(expr->entity());
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

    case Null:
        lit.decorateValue(sym.temporary(sym.NullPtrType()), RValue);
        return &lit;

    case This: {
        auto* scope = &sym.currentScope();
        while (!isa<Function>(scope)) {
            scope = scope->parent();
        }
        auto* function = cast<Function*>(scope);
        auto* thisEntity = function->findProperty(PropertyKind::This);
        lit.decorateValue(thisEntity, LValue, thisEntity->getQualType());
        return &lit;
    }
    case String: {
        /// We deliberately derive string literals as `&str` and not as 
        /// `&[byte, N]` 
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
    if (!analyzeValue(u.operand())) {
        return nullptr;
    }
    auto* type = u.operand()->type().get();
    switch (u.operation()) {
    case ast::UnaryOperator::Promotion:
        [[fallthrough]];
    case ast::UnaryOperator::Negation:
        if (!isAny<IntType, FloatType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(type), RValue);
        break;
    case ast::UnaryOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(type), RValue);
        break;
    case ast::UnaryOperator::LogicalNot:
        if (!isAny<BoolType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(type), RValue);
        break;
    case ast::UnaryOperator::Increment:
        [[fallthrough]];
    case ast::UnaryOperator::Decrement: {
        if (!isAny<IntType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        if (!u.operand()->isLValue()) {
            ctx.badExpr(&u, UnaryExprValueCat);
            return nullptr;
        }
        if (!u.operand()->type().isMut()) {
            ctx.badExpr(&u, UnaryExprImmutable);
            return nullptr;
        }
        switch (u.notation()) {
        case ast::UnaryOperatorNotation::Prefix: {
            u.decorateValue(u.operand()->entity(), LValue, type);
            break;
        }
        case ast::UnaryOperatorNotation::Postfix: {
            u.decorateValue(sym.temporary(type), RValue);
            break;
        }
        }
        break;
    }
    }
    u.setConstantValue(evalUnary(u.operation(), u.operand()->constantValue()));
    return &u;
}

static ObjectType const* getResultType(SymbolTable& sym,
                                       ObjectType const* type,
                                       ast::BinaryOperator op) {
    /// For arighmetic assignment we recursively call this function with the
    /// respective non-assignment arithemetic operator
    if (ast::isArithmeticAssignment(op)) {
        if (getResultType(sym, type, ast::toNonAssignment(op))) {
            return sym.Void();
        }
        return nullptr;
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
        return nullptr;

    case Remainder:
        if (isAny<IntType>(type)) {
            return type;
        }
        return nullptr;

    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr:
        if (isAny<ByteType, BoolType, IntType>(type)) {
            return type;
        }
        return nullptr;

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        if (isAny<ByteType, IntType, FloatType, NullPtrType, PointerType>(type))
        {
            return sym.Bool();
        }
        return nullptr;

    case Equals:
        [[fallthrough]];
    case NotEquals:
        if (isAny<ByteType,
                  BoolType,
                  IntType,
                  FloatType,
                  NullPtrType,
                  PointerType>(type))
        {
            return sym.Bool();
        }
        return nullptr;

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr:
        if (isAny<BoolType>(type)) {
            return sym.Bool();
        }
        return nullptr;

    case LeftShift:
        [[fallthrough]];
    case RightShift:
        if (isAny<ByteType, IntType>(type)) {
            return type;
        }
        return nullptr;

    case Assignment:
        return sym.Void();

    default:
        SC_UNREACHABLE();
    }
}

static void setConstantValue(ast::BinaryExpression& expr) {
    expr.setConstantValue(evalBinary(expr.operation(),
                                     expr.lhs()->constantValue(),
                                     expr.rhs()->constantValue()));
}

ast::Expression* ExprContext::analyzeImpl(ast::BinaryExpression& expr) {
    bool argsAnalyzed = true;
    argsAnalyzed &= !!analyzeValue(expr.lhs());
    argsAnalyzed &= !!analyzeValue(expr.rhs());
    if (!argsAnalyzed) {
        return nullptr;
    }

    /// Handle comma operator separately
    if (expr.operation() == ast::BinaryOperator::Comma) {
        expr.decorateValue(expr.rhs()->object(),
                           expr.rhs()->valueCategory(),
                           expr.rhs()->type());
        setConstantValue(expr);
        return &expr;
    }

    /// Determine common type of operands
    QualType commonType =
        ast::isAssignment(expr.operation()) ?
            expr.lhs()->type() :
            sema::commonType(sym, expr.lhs()->type(), expr.rhs()->type());
    if (!commonType) {
        ctx.badExpr(&expr, BinaryExprNoCommonType);
        return nullptr;
    }

    /// Determine result type of operation. We do this before we handle
    /// assignments because arithmetic assignment may be invalid for the operand
    /// types
    auto* resultType = getResultType(sym, commonType.get(), expr.operation());
    if (!resultType) {
        ctx.badExpr(&expr, BinaryExprBadType);
        return nullptr;
    }

    /// Handle assignment seperately
    if (ast::isAssignment(expr.operation())) {
        QualType lhsType = expr.lhs()->type();
        if (!lhsType->hasTrivialLifetime()) {
            auto assign = allocate<ast::NonTrivAssignExpr>(expr.extractLHS(),
                                                           expr.extractRHS());
            analyze(assign.get());
            return expr.parent()->replaceChild(&expr, std::move(assign));
        }
        if (expr.lhs()->valueCategory() != LValue) {
            ctx.badExpr(&expr, BinaryExprValueCatLHS);
            return nullptr;
        }
        if (!expr.lhs()->type().isMut()) {
            ctx.badExpr(&expr, BinaryExprImmutableLHS);
            return nullptr;
        }
        if (!convert(Implicit, expr.rhs(), lhsType, RValue, *dtorStack, ctx)) {
            return nullptr;
        }
        expr.decorateValue(sym.temporary(sym.Void()), RValue);
        setConstantValue(expr);
        return &expr;
    }

    /// Convert the operands to common type
    bool cnv = true;
    cnv &= !!convert(Implicit, expr.lhs(), commonType, RValue, *dtorStack, ctx);
    cnv &= !!convert(Implicit, expr.rhs(), commonType, RValue, *dtorStack, ctx);
    SC_ASSERT(cnv, "Conversion must succeed because we have a common type");
    expr.decorateValue(sym.temporary(resultType), RValue);
    setConstantValue(expr);
    return &expr;
}

static Scope* findMALookupScope(ast::Identifier& idExpr) {
    auto* maParent = dyncast<ast::MemberAccess*>(idExpr.parent());
    if (!maParent || maParent->member() != &idExpr) {
        return nullptr;
    }
    auto& accessed = *maParent->accessed();
    switch (accessed.entityCategory()) {
    case EntityCategory::Value: {
        return const_cast<ObjectType*>(accessed.type().get());
    }
    case EntityCategory::Type:
        return cast<Scope*>(accessed.entity());
    default:
        SC_UNREACHABLE();
    }
}

/// If \p entities
/// - is empty, returns null
/// - has one element, returns that element
/// - has multiple elements, checks that all elements are functions and returns
///   an overload set
static Entity* toSingleEntity(ast::Identifier const& idExpr,
                              std::span<Entity* const> entities,
                              AnalysisContext& ctx) {
    if (entities.empty()) {
        return nullptr;
    }
    // TODO: Remove the check for !isa<Function>
    if (entities.size() == 1 && !isa<Function>(entities.front())) {
        return entities.front();
    }
    if (!ranges::all_of(entities, isa<Function>)) {
        // TODO: Push error
        SC_UNIMPLEMENTED();
    }
    auto functions = entities | ranges::views::transform(cast<Function*>) |
                     ToSmallVector<>;
    return ctx.symbolTable().addOverloadSet(idExpr.sourceRange(),
                                            std::move(functions));
}

ast::Expression* ExprContext::analyzeImpl(ast::Identifier& idExpr) {
    auto entities = [&] {
        if (auto* scope = findMALookupScope(idExpr)) {
            return scope->findEntities(idExpr.value()) | ToSmallVector<>;
        }
        else {
            return sym.unqualifiedLookup(idExpr.value());
        }
    }();
    auto* entity = toSingleEntity(idExpr, entities, ctx);
    if (!entity) {
        ctx.badExpr(&idExpr, UndeclaredID);
        return nullptr;
    }
    // clang-format off
    return SC_MATCH (*entity) {
        [&](VarBase& var) -> ast::Expression* {
            if (isa<Type>(var.parent()) &&
                !isa<ast::MemberAccess>(idExpr.parent()))
            {
                ctx.badExpr(&idExpr, AccessedMemberWithoutObject);
                return nullptr;
            }
            idExpr.decorateValue(&var, var.valueCategory(), var.getQualType());
            idExpr.setConstantValue(clone(var.constantValue()));
            return &idExpr;
        },
        [&](ObjectType& type) {
            idExpr.decorateType(&type);
            return &idExpr;
        },
        [&](OverloadSet& overloadSet) {
            idExpr.decorateValue(&overloadSet, LValue);
            if (!isa<ast::FunctionCall>(idExpr.parent()) &&
                !isa<ast::MemberAccess>(idExpr.parent()))
            {
                ctx.badExpr(&idExpr, BadExpr::GenericBadExpr);
            }
            return &idExpr;
        },
        [&](Generic& generic) {
            idExpr.decorateValue(&generic, LValue);
            return &idExpr;
        },
        [&](PoisonEntity& poison) {
            idExpr.decorateValue(&poison, RValue);
            return nullptr;
        },
        [&](Entity const& entity) -> ast::Expression* {
            /// Other entities can not be referenced directly
            SC_UNREACHABLE();
        }
    }; // clang-format on
}

ast::Expression* ExprContext::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(ma.accessed())) {
        return nullptr;
    }
    dereferencePointer(ma.accessed());
    if (!analyze(ma.member())) {
        return nullptr;
    }
    /// Double dispatch table on the entity categories of the accessed object
    /// and the member
    switch (ma.accessed()->entityCategory()) {
    case EntityCategory::Value: {
        // clang-format off
        return SC_MATCH (*ma.member()->entity()) {
            [&](Object& object) {
                auto mut = ma.accessed()->type().mutability();
                auto type = ma.member()->type().to(mut);
                ma.decorateValue(sym.temporary(type),
                                 ma.member()->valueCategory());
                ma.setConstantValue(clone(ma.member()->constantValue()));
                return &ma;
            },
            [&](OverloadSet& os) {
                ma.decorateValue(&os, LValue);
                if (!isa<ast::FunctionCall>(ma.parent())) {
                    ctx.badExpr(&ma, BadExpr::GenericBadExpr);
                }
                return &ma;
            },
            [&](Type& type) {
                ctx.badExpr(&ma, MemAccTypeThroughValue);
                return nullptr;
            },
            [&](Entity& type) -> ast::Expression* {
                SC_UNREACHABLE();
            }
        }; // clang-format on
    }
    case EntityCategory::Type:
        // clang-format off
        return SC_MATCH (*ma.member()->entity()) {
            [&](Object& object) {
                ctx.badExpr(&ma, MemAccNonStaticThroughType);
                return nullptr;
            },
            [&](OverloadSet& os) {
                ma.decorateValue(&os, LValue);
                return &ma;
            },
            [&](Type& type) {
                ma.decorateType(&type);
                return &ma;
            },
            [&](Entity& type) -> ast::Expression* {
                SC_UNREACHABLE();
            }
        }; // clang-format on
    case EntityCategory::Indeterminate:
        return nullptr;
    }
}

///
static QualType getPtrBase(ObjectType const* type) {
    if (auto* ptrType = dyncast<PointerType const*>(type)) {
        return ptrType->base();
    }
    return nullptr;
}

ast::Expression* ExprContext::analyzeImpl(ast::DereferenceExpression& expr) {
    if (!analyze(expr.referred())) {
        return nullptr;
    }
    auto* pointer = expr.referred();
    switch (pointer->entityCategory()) {
    case EntityCategory::Value: {
        auto baseType = getPtrBase(pointer->type().get());
        if (!baseType) {
            ctx.badExpr(&expr, DerefNoPtr);
            return nullptr;
        }
        if (expr.unique()) {
            ctx.badExpr(&expr, GenericBadExpr);
            return nullptr;
        }
        expr.decorateValue(sym.temporary(baseType), LValue);
        return &expr;
    }
    case EntityCategory::Type: {
        auto* type = cast<ObjectType*>(pointer->entity());
        auto pointee = QualType(type, expr.mutability());
        if (expr.unique()) {
            auto* ptrType = sym.uniquePointer(pointee);
            expr.decorateType(const_cast<UniquePtrType*>(ptrType));
        }
        else {
            auto* ptrType = sym.pointer(pointee);
            expr.decorateType(const_cast<RawPtrType*>(ptrType));
        }
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
            ctx.badExpr(&expr, AddrOfNoLValue);
            return nullptr;
        }
        if (!isConvertible(referred->type().mutability(), expr.mutability())) {
            ctx.badExpr(&expr, MutAddrOfImmutable);
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
        ctx.badExpr(&c, ConditionalNoCommonType);
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

ast::Expression* ExprContext::analyzeImpl(ast::MoveExpr& expr) {
    if (!analyzeValue(expr.value())) {
        return nullptr;
    }
    auto type = expr.value()->type();
    if (type.isConst()) {
        ctx.badExpr(&expr, BadExpr::MoveExprConst);
        return nullptr;
    }
    if (expr.value()->isRValue()) {
        ctx.badExpr(&expr, BadExpr::MoveExprRValue);
        expr.decorateValue(expr.value()->object(), RValue);
        return &expr;
    }
    if (!type->hasTrivialLifetime()) {
        using enum SpecialLifetimeFunction;
        auto* moveCtor = type->specialLifetimeFunction(MoveConstructor);
        auto* copyCtor = type->specialLifetimeFunction(CopyConstructor);
        auto* ctor = moveCtor ? moveCtor : copyCtor;
        if (!ctor) {
            ctx.badExpr(&expr, BadExpr::MoveExprImmovable);
            return nullptr;
        }
        if (ctor == copyCtor) {
            ctx.badExpr(&expr, BadExpr::MoveExprCopies);
        }
        expr.setFunction(ctor);
    }
    expr.decorateValue(sym.temporary(expr.value()->type()), RValue);
    dtorStack->push(expr.object());
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::UniqueExpr& expr) {
    if (!analyzeValue(expr.value())) {
        return nullptr;
    }
    if (!expr.value()->isRValue()) {
        ctx.badExpr(&expr, UniqueExprNoRValue);
        return nullptr;
    }
    /// We pop the top level dtor because unique ptr extends the lifetime of the
    /// object
    popTopLevelDtor(expr.value(), *dtorStack);
    auto* type = sym.uniquePointer(expr.value()->type());
    expr.decorateValue(sym.temporary(type), RValue);
    dtorStack->push(expr.object());
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::Subscript& expr) {
    auto* arrayType = analyzeSubscriptCommon(expr);
    if (!arrayType) {
        return nullptr;
    }
    if (expr.arguments().size() != 1) {
        ctx.badExpr(&expr, SubscriptArgCount);
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
    dereferencePointer(expr.callee());
    for (auto* arg: expr.arguments()) {
        success &= !!analyzeValue(arg);
    }
    if (!success) {
        return nullptr;
    }
    auto* accessedType = expr.callee()->type().get();
    auto* arrayType = dyncast<ArrayType const*>(accessedType);
    if (!arrayType) {
        ctx.badExpr(&expr, SubscriptNoArray);
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

static void convertArguments(auto const& arguments,
                             auto const& conversions,
                             DtorStack& dtors,
                             AnalysisContext& ctx) {
    for (auto [arg, conv]: ranges::views::zip(arguments, conversions)) {
        insertConversion(arg, conv, dtors, ctx);
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::FunctionCall& fc) {
    bool success = analyze(fc.callee());
    auto orKind = ORKind::FreeFunction;
    /// We analyze all the arguments
    for (size_t i = 0; i < fc.arguments().size(); ++i) {
        success &= !!analyzeValue(fc.argument(i));
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
        orKind = ORKind::MemberFunction;
    }

    /// If our callee is a generic expression, we assert that it is a
    /// `reinterpret` expression and rewrite the AST
    if (auto* genExpr = dyncast<ast::GenericExpression*>(fc.callee())) {
        SC_ASSERT(genExpr->callee()->entity()->name() == "reinterpret", "");
        if (fc.arguments().size() != 1) {
            ctx.badExpr(&fc, GenericBadExpr);
            return nullptr;
        }
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
        auto owner = allocate<ast::ConstructExpr>(std::move(args),
                                                  targetType,
                                                  fc.sourceRange());
        auto* constructExpr = fc.parent()->replaceChild(&fc, std::move(owner));
        return analyzeValue(constructExpr);
    }

    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast<OverloadSet*>(fc.callee()->entity());
    if (!overloadSet) {
        ctx.badExpr(&fc, ObjectNotCallable);
        return nullptr;
    }

    /// Perform overload resolution
    auto result = performOverloadResolution(&fc,
                                            *overloadSet,
                                            fc.arguments() | ToSmallVector<>,
                                            orKind);
    if (result.error) {
        result.error->setSourceRange(fc.sourceRange());
        ctx.issueHandler().push(std::move(result.error));
        return nullptr;
    }
    auto* function = result.function;
    /// Cannot explicitly call special member functions
    if (function->isSpecialMemberFunction()) {
        ctx.badExpr(&fc, ExplicitSMFCall);
        return nullptr;
    }
    /// Check if return type deduction succeeded
    auto* returnType = getReturnType(function);
    if (!returnType) {
        ctx.badExpr(&fc, CantDeduceReturnType);
        return nullptr;
    }
    QualType type = getQualType(returnType);
    auto valueCat = isa<ReferenceType>(*returnType) ? LValue : RValue;
    fc.decorateCall(sym.temporary(type), valueCat, type, function);
    convertArguments(fc.arguments(), result.conversions, *dtorStack, ctx);
    if (valueCat == RValue) {
        dtorStack->push(fc.object());
    }
    return &fc;
}

ast::Expression* ExprContext::analyzeImpl(ast::ListExpression& list) {
    bool success = true;
    for (auto* expr: list.elements()) {
        expr = analyze(expr);
        success &= !!expr;
        popTopLevelDtor(expr, *dtorStack);
    }
    if (!success) {
        return nullptr;
    }
    if (list.elements().empty()) {
        /// For now until we implement another way to deduce list types
        /// The problem is if we don't push an error here, a variable with an
        /// empty list expression as it's initializer would not emit an error,
        /// as we try to avoid emitting duplicate errors.
        ctx.badExpr(&list, GenericBadExpr);
        return nullptr;
    }
    auto* first = list.elements().front();
    switch (first->entityCategory()) {
    case EntityCategory::Value: {
        for (auto* expr: list.elements()) {
            success &= expectValue(expr);
        }
        if (!success) {
            return nullptr;
        }
        auto elements = list.elements() | ToSmallVector<>;
        QualType commonType = sema::commonType(sym, elements);
        if (!commonType) {
            ctx.badExpr(&list, ListExprNoCommonType);
            return nullptr;
        }
        if (isa<VoidType>(*commonType)) {
            ctx.badExpr(&list, ListExprVoid);
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
        dtorStack->push(list.object());
        return &list;
    }
    case EntityCategory::Type: {
        auto* elementType = cast<ObjectType*>(first->entity());
        if (list.elements().size() > 2) {
            ctx.badExpr(&list, ListExprTypeExcessElements);
            return nullptr;
        }
        size_t count = ArrayType::DynamicCount;
        if (list.elements().size() == 2) {
            auto* countExpr = list.element(1);
            auto* countType = dyncast<IntType const*>(countExpr->type().get());
            if (!countType) {
                ctx.badExpr(countExpr, ListExprNoIntSize);
                return nullptr;
            }
            auto* value =
                cast_or_null<IntValue const*>(countExpr->constantValue());
            if (!value) {
                ctx.badExpr(countExpr, ListExprNoConstSize);
                return nullptr;
            }
            if (countType->isSigned() && value->value().negative()) {
                ctx.badExpr(countExpr, ListExprNegativeSize);
                return nullptr;
            }
            count = value->value().to<size_t>();
        }
        auto* arrayType = sym.arrayType(elementType, count);
        list.decorateType(const_cast<ArrayType*>(arrayType));
        return &list;
    }
    case EntityCategory::Indeterminate:
        return nullptr;
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::NonTrivAssignExpr& expr) {
    bool argsAnalyzed = true;
    argsAnalyzed &= !!analyzeValue(expr.dest());
    argsAnalyzed &= !!analyzeValue(expr.source());
    if (!argsAnalyzed) {
        return nullptr;
    }
    if (expr.dest()->type().get() != expr.source()->type().get()) {
        auto* conv = convert(Implicit,
                             expr.source(),
                             expr.dest()->type(),
                             RValue,
                             *dtorStack,
                             ctx);
        if (!conv) {
            return nullptr;
        }
    }
    using enum SpecialLifetimeFunction;
    auto* type = expr.dest()->type().get();
    auto* copyCtor = type->specialLifetimeFunction(CopyConstructor);
    auto* moveCtor = type->specialLifetimeFunction(MoveConstructor);
    auto* dtor = type->specialLifetimeFunction(Destructor);
    if (expr.source()->isLValue()) {
        if (!copyCtor) {
            ctx.issue<BadExpr>(&expr, CannotAssignUncopyableType);
            return nullptr;
        }
        expr.decorateAssign(sym.temporary(sym.Void()), dtor, copyCtor);
    }
    else {
        auto* ctor = moveCtor ? moveCtor : copyCtor;
        if (!ctor) {
            ctx.issue<BadExpr>(&expr, CannotAssignUncopyableType);
            return nullptr;
        }
        expr.decorateAssign(sym.temporary(sym.Void()), dtor, ctor);
    }
    return &expr;
}

static Object* getConvertedEntity(Entity* original,
                                  Conversion const& conv,
                                  SymbolTable& sym) {
    if (conv.objectConversion() == ObjectTypeConversion::None ||
        conv.objectConversion() == ObjectTypeConversion::Array_FixedToDynamic)
    {
        return cast<Object*>(original);
    }
    return sym.temporary(conv.targetType());
}

static ValueCategory getValueCat(ValueCategory original,
                                 ValueCatConversion conv) {
    using enum ValueCatConversion;
    switch (conv) {
    case None:
        return original;
    case LValueToRValue:
        return RValue;
    case MaterializeTemporary:
        return LValue;
    }
}

/// Guaranteed to succeed as long as the converted value has no issues because
/// we only insert conversion nodes if conversions are valid
ast::Expression* ExprContext::analyzeImpl(ast::Conversion& expr) {
    if (!analyzeValue(expr.expression())) {
        return nullptr;
    }
    auto* conv = expr.conversion();
    auto* entity = getConvertedEntity(expr.expression()->entity(), *conv, sym);
    expr.decorateValue(entity,
                       getValueCat(expr.expression()->valueCategory(),
                                   conv->valueCatConversion()),
                       conv->targetType());
    if (conv->objectConversion() == ObjectTypeConversion::NullPtrToUniquePtr) {
        dtorStack->push(entity);
    }
    expr.setConstantValue(
        evalConversion(conv, expr.expression()->constantValue()));
    return &expr;
}

/// Decides whether the constructor call is a pseudo constructor call or an
/// actual constructor call. This is probably a half baked solution and should
/// be revisited.
static bool ctorIsPseudo(ObjectType const* type, auto const& args) {
    /// We just assume pseudo constructor instead of asserting non-null
    if (!type) {
        return true;
    }
    /// Non-trivial lifetime type has no pseudo constructors
    if (!type->hasTrivialLifetime()) {
        return false;
    }
    /// Trivial lifetime copy constructor call
    if (args.size() == 1 && args.front()->type().get() == type) {
        return true;
    }
    /// Trivial lifetime general constructor call
    using enum SpecialMemberFunction;
    return type->specialMemberFunctions(New).empty();
}

static bool canConstructTrivialType(ast::ConstructExpr& expr,
                                    DtorStack& dtors,
                                    AnalysisContext& ctx) {
    auto* type = expr.constructedType();
    auto arguments = expr.arguments();
    if (arguments.size() == 0) {
        return true;
    }
    if (arguments.size() == 1 &&
        (!isa<StructType>(type) || type == arguments.front()->type().get()))
    {
        /// We convert explicitly here because it allows expressions like
        /// `int(1.0)`
        return !!convert(Explicit,
                         arguments.front(),
                         getQualType(type, Mutability::Const),
                         LValue, // So we don't end up in infinite recursion :/
                         dtors,
                         ctx);
    }
    if (auto* structType = dyncast<StructType const*>(type)) {
        if (arguments.size() != structType->members().size()) {
            ctx.badExpr(&expr, CannotConstructType);
            return false;
        }
        bool success = true;
        for (auto [arg, type]:
             ranges::views::zip(arguments, structType->members()))
        {
            success &= !!convert(Implicit,
                                 arg,
                                 getQualType(type, Mutability::Const),
                                 RValue,
                                 dtors,
                                 ctx);
        }
        return success;
    }
    return false;
}

ast::Expression* ExprContext::analyzeImpl(ast::ConstructExpr& expr) {
    bool success = true;
    for (auto* arg: expr.arguments()) {
        success &= !!analyzeValue(arg);
    }
    if (!success) {
        return nullptr;
    }
    auto* type = expr.constructedType();
    /// Trivial case
    if (ctorIsPseudo(type, expr.arguments())) {
        if (!canConstructTrivialType(expr, *dtorStack, ctx)) {
            return nullptr;
        }
        expr.decorateConstruct(sym.temporary(type), nullptr);
        return &expr;
    }
    /// Non-trivial case
    if (expr.arguments().empty() ||
        !isa<ast::UninitTemporary>(expr.argument(0)))
    {
        auto obj = allocate<ast::UninitTemporary>(expr.sourceRange());
        obj->decorateValue(sym.temporary(type), LValue);
        expr.insertArgument(0, std::move(obj));
    }
    using enum SpecialMemberFunction;
    auto ctorSet = type->specialMemberFunctions(New);
    SC_ASSERT(!ctorSet.empty(), "Trivial lifetime case is handled above");
    auto result = performOverloadResolution(&expr,
                                            ctorSet,
                                            expr.arguments() | ToAddress |
                                                ToSmallVector<>,
                                            ORKind::MemberFunction);
    if (result.error) {
        result.error->setSourceRange(expr.sourceRange());
        ctx.issueHandler().push(std::move(result.error));
        return nullptr;
    }
    convertArguments(expr.arguments(), result.conversions, *dtorStack, ctx);
    expr.decorateConstruct(sym.temporary(type), result.function);
    dtorStack->push(expr.object());
    return &expr;
}

void ExprContext::dereferencePointer(ast::Expression* expr) {
    if (!expr || !expr->isDecorated()) {
        return;
    }
    auto* type = expr->type().get();
    if (!isa<PointerType const*>(type)) {
        return;
    }
    SC_EXPECT(expr->isValue());
    auto* parent = expr->parent();
    SC_EXPECT(parent);
    size_t index = expr->indexInParent();
    auto deref = allocate<ast::DereferenceExpression>(expr->extractFromParent(),
                                                      Mutability::Const,
                                                      /* unique = */ false,
                                                      expr->sourceRange());
    bool result = analyzeValue(deref.get());
    SC_ASSERT(result, "How can a pointer dereference fail?");
    parent->setChild(index, std::move(deref));
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
    if (!expr || !expr->isDecorated()) {
        return false;
    }
    if (!expr->isValue()) {
        ctx.issue<BadSymRef>(expr, EntityCategory::Value);
        return false;
    }
    if (!expr->type()) {
        return false;
    }
    return true;
}

bool ExprContext::expectType(ast::Expression const* expr) {
    if (!expr || !expr->isDecorated()) {
        return false;
    }
    if (!expr->isType()) {
        ctx.issue<BadSymRef>(expr, EntityCategory::Type);
        return false;
    }
    return true;
}
