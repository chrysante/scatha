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
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Analysis/StatementAnalysis.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/SemaIssues.h"
#include "Sema/SymbolTable.h"
#include "Sema/ThinExpr.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;
using enum ValueCategory;
using enum ConversionKind;
using enum BadExpr::Reason;

namespace {

struct ExprContext {
    CleanupStack* _cleanupStack;
    sema::AnalysisContext& ctx;
    SymbolTable& sym;

    utl::stack<Type const*> fstringFormatTypeStack = {};

    ///
    CleanupStack& currentCleanupStack() {
        SC_EXPECT(_cleanupStack);
        return *_cleanupStack;
    }

    ///
    void setCurrentCleanupStack(CleanupStack& cleanupStack) {
        _cleanupStack = &cleanupStack;
    }

    ExprContext(sema::AnalysisContext& ctx, CleanupStack* cleanupStack):
        _cleanupStack(cleanupStack), ctx(ctx), sym(ctx.symbolTable()) {}

    SC_NODEBUG ast::Expression* analyze(ast::Expression*);

    /// Shorthand for `analyze() && expectValue()`
    ast::Expression* analyzeValue(ast::Expression*);

    /// Analyzes a range of value expressions
    bool analyzeValues(auto&& exprs);

    ///
    ast::Expression* analyzeType(ast::Expression* expr);

    ast::Expression* analyzeImpl(ast::Literal&);
    ast::Expression* analyzeImpl(ast::FStringExpr&);
    Function const* analyzeFStringOperand(ast::Expression* expr);
    Function const* analyzeFStringOperandImpl(ObjectType const&,
                                              ast::Expression*);
    Function const* analyzeFStringOperandImpl(StructType const&,
                                              ast::Expression*);
    Function const* analyzeFStringOperandImpl(PointerType const&,
                                              ast::Expression*);
    Function const* analyzeFStringOperandImpl(IntType const&, ast::Expression*);
    Function const* analyzeFStringOperandImpl(FloatType const&,
                                              ast::Expression*);
    Function const* analyzeFStringOperandImpl(ByteType const&,
                                              ast::Expression*);
    Function const* analyzeFStringOperandImpl(BoolType const&,
                                              ast::Expression*);
    ast::Expression* analyzeImpl(ast::UnaryExpression&);
    ast::Expression* analyzeImpl(ast::BinaryExpression&);
    ast::Expression* analyzeImpl(ast::Identifier&);
    bool validateAccessPermission(Entity const& entity) const;
    ast::Expression* analyzeImpl(ast::CastExpr&);
    ast::Expression* analyzeImpl(ast::MemberAccess&);
    ast::Expression* analyzeImpl(ast::DereferenceExpression&);
    ast::Expression* analyzeImpl(ast::AddressOfExpression&);
    ast::Expression* analyzeImpl(ast::Conditional&);
    ast::Expression* analyzeImpl(ast::MoveExpr&);
    ast::Expression* analyzeImpl(ast::UniqueExpr&);
    ast::Expression* analyzeImpl(ast::FunctionCall&);
    ast::Expression* analyzeImpl(ast::Subscript&);
    ast::Expression* analyzeImpl(ast::SubscriptSlice&);
    ArrayType const* analyzeSubscriptCommon(ast::CallLike&);
    ast::Expression* analyzeImpl(ast::GenericExpression&);
    ast::Expression* analyzeImpl(ast::ListExpression&);
    ast::Expression* analyzeImpl(ast::NontrivAssignExpr&);
    ast::Expression* analyzeImpl(ast::ValueCatConvExpr&);
    ast::Expression* analyzeImpl(ast::MutConvExpr&);
    ast::Expression* analyzeImpl(ast::ObjTypeConvExpr&);
    ast::Expression* analyzeImpl(ast::TrivDefConstructExpr&);
    ast::Expression* analyzeImpl(ast::TrivCopyConstructExpr&);
    ast::Expression* analyzeImpl(ast::TrivAggrConstructExpr&);
    ast::Expression* analyzeImpl(ast::NontrivConstructExpr&);
    ast::Expression* analyzeImpl(ast::NontrivInlineConstructExpr&);
    ast::Expression* analyzeImpl(ast::NontrivAggrConstructExpr&);
    ast::Expression* analyzeImpl(ast::DynArrayConstructExpr&);

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
                                         CleanupStack& cleanupStack,
                                         AnalysisContext& ctx) {
    return ExprContext(ctx, &cleanupStack).analyze(expr);
}

ast::Expression* sema::analyzeValueExpr(ast::Expression* expr,
                                        CleanupStack& cleanupStack,
                                        AnalysisContext& ctx) {
    return ExprContext(ctx, &cleanupStack).analyzeValue(expr);
}

ast::Expression* sema::analyzeTypeExpr(ast::Expression* expr,
                                       AnalysisContext& ctx) {
    return ExprContext(ctx, nullptr).analyzeType(expr);
}

static QualType deducePointerBase(ast::Expression const* typeExpr,
                                  TypeDeductionQualifier const* qual,
                                  ast::Expression const* valueExpr,
                                  QualType initType, AnalysisContext& ctx) {
    SC_EXPECT(initType);
    auto* pointerType = dyncast<PointerType const*>(initType.get());
    if (!pointerType) {
        ctx.issue<BadTypeDeduction>(typeExpr, valueExpr,
                                    BadTypeDeduction::NoPointer);
        return nullptr;
    }
    auto base = pointerType->base();
    if (qual->isMutable() && base.isConst()) {
        ctx.issue<BadTypeDeduction>(typeExpr, valueExpr,
                                    BadTypeDeduction::Mutability);
        return nullptr;
    }
    return base.to(qual->mutability());
}

Type const* sema::deduceType(ast::Expression* typeExpr,
                             ast::Expression* valueExpr, AnalysisContext& ctx) {
    SC_EXPECT(!typeExpr || typeExpr->isDecorated());
    SC_EXPECT(!valueExpr || valueExpr->isDecorated());
    auto& sym = ctx.symbolTable();
    auto* declEntity = typeExpr ? typeExpr->entity() : nullptr;
    auto initType = valueExpr ? valueExpr->type() : nullptr;
    if (auto* declType = dyncast<Type const*>(declEntity)) {
        return declType;
    }
    if (auto* qual = dyncast<TypeDeductionQualifier const*>(declEntity)) {
        if (!initType) {
            ctx.issue<BadTypeDeduction>(typeExpr, nullptr,
                                        BadTypeDeduction::MissingInitializer);
            return nullptr;
        }
        switch (qual->refKind()) {
        case ReferenceKind::Reference:
            if (qual->isMutable() && initType.isConst()) {
                ctx.issue<BadTypeDeduction>(typeExpr, valueExpr,
                                            BadTypeDeduction::Mutability);
                return nullptr;
            }
            return sym.reference(initType.to(qual->mutability()));
        case ReferenceKind::Pointer: {
            auto base =
                deducePointerBase(typeExpr, qual, valueExpr, initType, ctx);
            return base ? sym.pointer(base) : nullptr;
        }
        case ReferenceKind::UniquePointer:
            auto base =
                deducePointerBase(typeExpr, qual, valueExpr, initType, ctx);
            return base ? sym.uniquePointer(base) : nullptr;
        }
    }
    return initType.get();
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
    return visit(*expr,
                 [this](auto&& e) SC_NODEBUG { return this->analyzeImpl(e); });
}

ast::Expression* ExprContext::analyzeValue(ast::Expression* expr) {
    auto* result = analyze(expr);
    if (!expectValue(result)) {
        return nullptr;
    }
    return result;
}

bool ExprContext::analyzeValues(auto&& exprs) {
    bool success = true;
    for (auto* expr: exprs) {
        success &= !!analyzeValue(expr);
    }
    return success;
}

ast::Expression* ExprContext::analyzeType(ast::Expression* expr) {
    /// Annoying hack. Can't use `currentCleanupStack()` here because that
    /// returns by reference and `_cleanupStack` may be null here
    auto* stashed = _cleanupStack;
    CleanupStack tmpDtorStack;
    setCurrentCleanupStack(tmpDtorStack);
    expr = analyze(expr);
    if (expr && expr->isDecorated() && expr->isType()) {
        SC_ASSERT(!expr->isType() || currentCleanupStack().empty(),
                  "Type expression should not trigger destructor calls");
    }
    _cleanupStack = stashed;
    if (!expectType(expr)) {
        return nullptr;
    }
    return expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::Literal& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        lit.decorateValue(sym.temporary(&lit, sym.S64()), RValue);
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ true));
        return &lit;

    case Boolean:
        lit.decorateValue(sym.temporary(&lit, sym.Bool()), RValue);
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return &lit;

    case FloatingPoint:
        lit.decorateValue(sym.temporary(&lit, sym.F64()), RValue);
        lit.setConstantValue(allocate<FloatValue>(lit.value<APFloat>()));
        return &lit;

    case Null:
        lit.decorateValue(sym.temporary(&lit, sym.NullPtr()), RValue);
        return &lit;

    case This: {
        auto* scope = &sym.currentScope();
        while (!isa<Function>(scope)) {
            scope = scope->parent();
        }
        auto* function = cast<Function*>(scope);
        auto* thisEntity = function->findProperty(PropertyKind::This);
        if (!thisEntity) {
            ctx.issue<BadExpr>(&lit, InvalidUseOfThis);
            return nullptr;
        }
        lit.decorateValue(thisEntity, LValue, thisEntity->getQualType());
        return &lit;
    }
    case String:
        [[fallthrough]];
    case FStringBegin:
        [[fallthrough]];
    case FStringContinue:
        [[fallthrough]];
    case FStringEnd: {
        /// We deliberately derive string literals as `*str` aka `*[byte]` and
        /// not as
        /// `*[byte, N]`
        auto type = sym.pointer(QualType::Const(sym.Str()));
        lit.decorateValue(sym.temporary(&lit, type), RValue);
        return &lit;
    }

    case Char:
        lit.decorateValue(sym.temporary(&lit, sym.Byte()), RValue);
        lit.setConstantValue(
            allocate<IntValue>(lit.value<APInt>(), /* signed = */ false));
        return &lit;
    }

    SC_UNREACHABLE();
}

ast::Expression* ExprContext::analyzeImpl(ast::FStringExpr& expr) {
    if (!analyzeValues(expr.operands())) {
        return nullptr;
    }
    utl::small_vector<Function const*> formatFunctions;
    ranges::transform(expr.operands(), std::back_inserter(formatFunctions),
                      [&](ast::Expression* expr) {
        return analyzeFStringOperand(expr);
    });
    auto* type = sym.uniquePointer(sym.Str(), Mutability::Mutable);
    auto* obj = sym.temporary(&expr, type);
    expr.decorateValue(obj, RValue, std::move(formatFunctions));
    if (!currentCleanupStack().push(obj, ctx)) {
        return nullptr;
    }
    return &expr;
}

Function const* ExprContext::analyzeFStringOperand(ast::Expression* expr) {
    fstringFormatTypeStack.push(expr->type().get());
    auto* F = visit(*expr->type(), [&](auto& type) {
        return analyzeFStringOperandImpl(type, expr);
    });
    fstringFormatTypeStack.pop();
    return F;
}

Function const* ExprContext::analyzeFStringOperandImpl(ObjectType const&,
                                                       ast::Expression* expr) {
    ctx.issue<BadExpr>(expr, NotFormattable);
    return nullptr;
}

Function const* ExprContext::analyzeFStringOperandImpl(StructType const&,
                                                       ast::Expression* expr) {
    size_t index = expr->indexInParent();
    auto* parent = expr->parent();
    auto funcID = allocate<ast::Identifier>(expr->sourceRange(), "to_string");
    auto callExpr =
        allocate<ast::FunctionCall>(std::move(funcID),
                                    toSmallVector(expr->extractFromParent()),
                                    expr->sourceRange());
    expr = callExpr.get();
    parent->setChild(index, std::move(callExpr));
    if (!(expr = analyzeValue(expr))) {
        return nullptr;
    }
    if (ranges::contains(fstringFormatTypeStack, expr->type().get())) {
        ctx.issue<BadExpr>(expr, NotFormattable);
        return nullptr;
    }
    return analyzeFStringOperand(expr);
}

static ast::Literal* asStringLiteral(ast::Expression* expr) {
    auto* lit = dyncast<ast::Literal*>(expr);
    if (!lit) {
        return nullptr;
    }
    using enum ast::LiteralKind;
    if (lit->kind() == String || lit->kind() == FStringBegin ||
        lit->kind() == FStringContinue || lit->kind() == FStringEnd)
    {
        return lit;
    }
    return nullptr;
}

Function const* ExprContext::analyzeFStringOperandImpl(PointerType const& type,
                                                       ast::Expression* expr) {
    if (auto* lit = asStringLiteral(expr);
        lit && lit->value<std::string>().size() == 0)
    {
        return nullptr;
    }
    if (auto* array = dyncast<ArrayType const*>(type.base().get());
        array && isa<ByteType>(array->elementType()))
    {
        convert(ConversionKind::Explicit, expr, sym.strPointer(), RValue,
                currentCleanupStack(), ctx);
        return sym.builtinFunction((size_t)svm::Builtin::fstring_writestr);
    }
    auto* target = sym.pointer(sym.arrayType(sym.Byte()), Mutability::Const);
    convert(ConversionKind::Reinterpret, expr, target, RValue,
            currentCleanupStack(), ctx);
    return sym.builtinFunction((size_t)svm::Builtin::fstring_writeptr);
}

Function const* ExprContext::analyzeFStringOperandImpl(IntType const& type,
                                                       ast::Expression* expr) {
    if (type.isSigned()) {
        convert(ConversionKind::Implicit, expr, sym.S64(), RValue,
                currentCleanupStack(), ctx);
        return sym.builtinFunction((size_t)svm::Builtin::fstring_writes64);
    }
    else {
        convert(ConversionKind::Implicit, expr, sym.U64(), RValue,
                currentCleanupStack(), ctx);
        return sym.builtinFunction((size_t)svm::Builtin::fstring_writeu64);
    }
}

Function const* ExprContext::analyzeFStringOperandImpl(FloatType const&,
                                                       ast::Expression* expr) {
    convert(ConversionKind::Implicit, expr, sym.F64(), RValue,
            currentCleanupStack(), ctx);
    return sym.builtinFunction((size_t)svm::Builtin::fstring_writef64);
}

Function const* ExprContext::analyzeFStringOperandImpl(ByteType const&,
                                                       ast::Expression*) {
    return sym.builtinFunction((size_t)svm::Builtin::fstring_writechar);
}

Function const* ExprContext::analyzeFStringOperandImpl(BoolType const&,
                                                       ast::Expression*) {
    return sym.builtinFunction((size_t)svm::Builtin::fstring_writebool);
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
        u.decorateValue(sym.temporary(&u, type), RValue);
        break;
    case ast::UnaryOperator::BitwiseNot:
        if (!isAny<ByteType, IntType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(&u, type), RValue);
        break;
    case ast::UnaryOperator::LogicalNot:
        if (!isAny<BoolType>(type)) {
            ctx.badExpr(&u, UnaryExprBadType);
            return nullptr;
        }
        u.decorateValue(sym.temporary(&u, type), RValue);
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
            u.decorateValue(sym.temporary(&u, type), RValue);
            break;
        }
        }
        break;
    }
    }
    u.setConstantValue(evalUnary(u.operation(), u.operand()->constantValue()));
    return &u;
}

static ObjectType const* getResultType(SymbolTable& sym, ObjectType const* type,
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
        if (isAny<ByteType, BoolType, IntType, FloatType, NullPtrType,
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

static QualType uniqueToRawPtr(QualType type, SymbolTable& sym) {
    if (auto* uniquePtr = dyncast<UniquePtrType const*>(type.get())) {
        return QualType(sym.pointer(uniquePtr->base()), type.mutability());
    }
    return type;
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
        expr.decorateValue(expr.rhs()->object(), expr.rhs()->valueCategory(),
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
        QualType rhsType = expr.rhs()->type();
        bool success = true;
        if (!lhsType->isComplete()) {
            ctx.badExpr(&expr, AssignExprIncompleteLHS);
            success = false;
        }
        if (!rhsType->isComplete()) {
            ctx.badExpr(&expr, AssignExprIncompleteRHS);
            success = false;
        }
        if (expr.lhs()->valueCategory() != LValue) {
            ctx.badExpr(&expr, AssignExprValueCatLHS);
            success = false;
        }
        if (!expr.lhs()->type().isMut()) {
            ctx.badExpr(&expr, AssignExprImmutableLHS);
            success = false;
        }
        if (!lhsType->hasTrivialLifetime()) {
            auto assign = allocate<ast::NontrivAssignExpr>(expr.extractLHS(),
                                                           expr.extractRHS());
            analyze(assign.get());
            return expr.parent()->replaceChild(&expr, std::move(assign));
        }
        if (!convert(Implicit, expr.rhs(), lhsType, RValue,
                     currentCleanupStack(), ctx))
        {
            success = false;
        }
        if (!success) {
            return nullptr;
        }
        expr.decorateValue(sym.temporary(&expr, sym.Void()), RValue);
        setConstantValue(expr);
        return &expr;
    }

    /// Convert the operands to common type
    bool cnv = true;
    cnv &= !!convert(Explicit, expr.lhs(), uniqueToRawPtr(commonType, sym),
                     RValue, currentCleanupStack(), ctx);
    cnv &= !!convert(Explicit, expr.rhs(), uniqueToRawPtr(commonType, sym),
                     RValue, currentCleanupStack(), ctx);
    SC_ASSERT(cnv, "Conversion must succeed because we have a common type");
    expr.decorateValue(sym.temporary(&expr, resultType), RValue);
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
    case EntityCategory::Value:
        return const_cast<ObjectType*>(accessed.type().get());
    case EntityCategory::Type:
        [[fallthrough]];
    case EntityCategory::Namespace:
        return dyncast<Scope*>(accessed.entity());
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
    if (entities.size() == 1 && !isa<Function>(stripAlias(entities.front()))) {
        return stripAlias(entities.front());
    }
    if (!ranges::all_of(entities, isa<Function>, stripAlias)) {
        // TODO: Find out how to trigger this
        SC_UNIMPLEMENTED();
    }
    auto functions = entities | transform(stripAlias) |
                     transform(cast<Function*>) | ToSmallVector<>;
    return ctx.symbolTable().addOverloadSet(idExpr.sourceRange(),
                                            std::move(functions));
}

/// \Returns `true` if \p var is a struct member and not declared `static`
static bool isNonStaticDataMember(sema::VarBase const& var) {
    // FIXME: This only tests if var is a struct data member, not if it is
    // non-static because we don't have static in the language yet
    return isa<Type>(var.parent());
}

/// \Returns `true` if \p expr if the right hand side operand of a member access
/// expression.
static bool isMemberAccessRHS(ast::Expression const& expr) {
    auto* parent = dyncast<ast::MemberAccess const*>(expr.parent());
    return parent && parent->member() == &expr;
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
            if (isNonStaticDataMember(var) && !isMemberAccessRHS(idExpr)) {
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
        [&](NativeLibrary& lib) {
            idExpr.decorateNamespace(&lib);
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
        [&](Entity const&) -> ast::Expression* {
            /// Other entities can not be referenced directly
            SC_UNREACHABLE();
        }
    }; // clang-format on
}

bool ExprContext::validateAccessPermission(Entity const& entity) const {
    /// If the entity is not private we return true because internal symbols are
    /// not exported to other liberaries, so they cannot even be found by name
    /// lookup. We could assert this assumption though
    if (!entity.isPrivate()) {
        return true;
    }
    auto* entityParent = entity.parent();
    auto* scope = &sym.currentScope();
    while (scope) {
        if (scope == entityParent) {
            return true;
        }
        scope = scope->parent();
    }
    return false;
}

ast::Expression* ExprContext::analyzeImpl(ast::CastExpr& expr) {
    bool success = true;
    success &= !!analyzeValue(expr.value());
    success &= !!analyzeType(expr.target());
    if (!success) {
        return nullptr;
    }
    auto* target = deduceType(expr.target(), expr.value(), ctx);
    if (!target) {
        return nullptr;
    }
    auto* conv = convert(ConversionKind::Explicit, expr.value(),
                         getQualType(target, Mutability::Mutable),
                         refToLValue(target), currentCleanupStack(), ctx);
    if (!conv) {
        return nullptr;
    }
    return expr.replace(conv->extractFromParent());
}

ast::Expression* ExprContext::analyzeImpl(ast::MemberAccess& ma) {
    if (!analyze(ma.accessed())) {
        return nullptr;
    }
    dereferencePointer(ma.accessed());
    if (!analyze(ma.member())) {
        return nullptr;
    }
    if (!validateAccessPermission(*ma.member()->entity())) {
        ctx.badExpr(ma.member(), AccessDenied);
    }
    /// Double dispatch table on the entity categories of the accessed object
    /// and the member
    auto entityCat = ma.accessed()->entityCategory();
    switch (entityCat) {
    case EntityCategory::Value: {
        // clang-format off
        return SC_MATCH (*ma.member()->entity()) {
            [&](Object&) {
                auto mut = ma.accessed()->type().mutability();
                auto type = ma.member()->type().to(mut);
                ma.decorateValue(sym.temporary(&ma, type),
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
            [&](Type&) {
                ctx.badExpr(&ma, MemAccTypeThroughValue);
                return nullptr;
            },
            [&](Entity&) -> ast::Expression* {
                SC_UNREACHABLE();
            }
        }; // clang-format on
    }
    case EntityCategory::Type:
        [[fallthrough]];
    case EntityCategory::Namespace:
        // clang-format off
        return SC_MATCH (*ma.member()->entity()) {
            [&](Object&) {
                ctx.badExpr(&ma, MemAccNonStaticThroughType);
                return nullptr;
            },
            [&](Variable& var) -> ast::Expression* {
                if (entityCat == EntityCategory::Type) {
                    /// We report an issue unconditionally here because we don't
                    /// have non-static variables yet
                    ctx.badExpr(&ma, MemAccNonStaticThroughType);
                    return nullptr;
                }
                ma.decorateValue(&var, LValue);
                return &ma;
            },
            [&](OverloadSet& os) {
                ma.decorateValue(&os, LValue);
                return &ma;
            },
            [&](Type& type) {
                ma.decorateType(&type);
                return &ma;
            },
            [&](Entity&) -> ast::Expression* {
                SC_UNREACHABLE();
            }
        }; // clang-format on
    case EntityCategory::Indeterminate:
        return nullptr;
    }

    SC_UNREACHABLE();
}

///
static QualType getPtrBase(ObjectType const* type) {
    if (auto* ptrType = dyncast<PointerType const*>(type)) {
        return ptrType->base();
    }
    return nullptr;
}

/// \Returns `Const` if both arguments are `Const`
static Mutability join(Mutability a, Mutability b) {
    return a == Mutability::Mutable ? a : b;
}

ast::Expression* ExprContext::analyzeImpl(ast::DereferenceExpression& expr) {
    if (expr.referred() && !analyze(expr.referred())) {
        return nullptr;
    }
    auto* pointer = expr.referred();
    auto cat = pointer ? pointer->entityCategory() : EntityCategory::Type;
    switch (cat) {
    case EntityCategory::Value: {
        SC_ASSERT(pointer, "");
        auto baseType = getPtrBase(pointer->type().get());
        if (!baseType) {
            ctx.badExpr(&expr, DerefNoPtr);
            return nullptr;
        }
        expr.decorateValue(sym.temporary(&expr, baseType), LValue);
        return &expr;
    }
    case EntityCategory::Type: {
        auto mut = expr.mutability();
        if (!pointer) {
            auto* qual =
                sym.typeDeductionQualifier(ReferenceKind::Pointer, mut);
            expr.decorateType(qual);
            return &expr;
        }
        /// `*unique` deduction qualifier
        if (isa<TypeDeductionQualifier>(pointer->entity())) {
            expr.decorateType(pointer->entity());
            return &expr;
        }
        auto* type = dyncast<ObjectType*>(pointer->entity());
        if (!type) {
            ctx.badExpr(&expr, PointerNoObjType);
            return nullptr;
        }
        if (auto* unique = dyncast<ast::UniqueExpr const*>(expr.referred())) {
            mut = ::join(mut, unique->mutability());
            auto* ptrType = sym.uniquePointer(QualType(type, mut));
            expr.decorateType(const_cast<UniquePtrType*>(ptrType));
        }
        else {
            auto* ptrType = sym.pointer(QualType(type, mut));
            expr.decorateType(const_cast<RawPtrType*>(ptrType));
        }
        return &expr;
    }
    default:
        ctx.issue<BadSymRef>(&expr, EntityCategory::Value);
        return nullptr;
    }
}

static bool isConvertible(Mutability from, Mutability to) {
    using enum Mutability;
    return from == Mutable || to == Const;
}

ast::Expression* ExprContext::analyzeImpl(ast::AddressOfExpression& expr) {
    if (expr.referred() && !analyze(expr.referred())) {
        return nullptr;
    }
    auto* referred = expr.referred();
    auto cat = referred ? referred->entityCategory() : EntityCategory::Type;
    switch (cat) {
    case EntityCategory::Value: {
        SC_ASSERT(referred, "");
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
        expr.decorateValue(sym.temporary(&expr, ptrType), RValue);
        return &expr;
    }
    case EntityCategory::Type: {
        auto mut = expr.mutability();
        if (!referred) {
            auto* qual =
                sym.typeDeductionQualifier(ReferenceKind::Reference, mut);
            expr.decorateType(qual);
            return &expr;
        }
        auto* type = dyncast<ObjectType*>(referred->entity());
        if (!type) {
            ctx.badExpr(&expr, ReferenceNoObjType);
            return nullptr;
        }
        auto* refType = sym.reference(QualType(type, mut));
        expr.decorateType(const_cast<ReferenceType*>(refType));
        return &expr;
    }
    default:
        /// Make an error class `InvalidReferenceExpression` and push that here
        ctx.issue<BadSymRef>(&expr, EntityCategory::Value);
        return nullptr;
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::Conditional& c) {
    if (analyzeValue(c.condition())) {
        convert(Implicit, c.condition(), sym.Bool(), RValue,
                currentCleanupStack(), ctx);
    }
    auto& commonCleanups = currentCleanupStack();
    auto& thenCleanups = c.branchCleanupStack(0);
    auto& elseCleanups = c.branchCleanupStack(1);

    bool success = true;
    setCurrentCleanupStack(thenCleanups);
    success &= !!analyzeValue(c.thenExpr());
    setCurrentCleanupStack(elseCleanups);
    success &= !!analyzeValue(c.elseExpr());
    setCurrentCleanupStack(commonCleanups);
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
    success &= !!convert(Implicit, c.thenExpr(), commonType, commonValueCat,
                         thenCleanups, ctx);
    success &= !!convert(Implicit, c.elseExpr(), commonType, commonValueCat,
                         elseCleanups, ctx);
    SC_ASSERT(success,
              "Common type should not return a type if not both types are "
              "convertible to that type");
    auto* obj = sym.temporary(&c, commonType);
#if 0 // Not yet sure about this
    if (commonValueCat == RValue) {
        popCleanup(c.thenExpr(), thenCleanups);
        popCleanup(c.elseExpr(), elseCleanups);
        commonCleanups.push(obj);
    }
#endif
    c.decorateValue(obj, commonValueCat);
    c.setConstantValue(evalConditional(c.condition()->constantValue(),
                                       c.thenExpr()->constantValue(),
                                       c.elseExpr()->constantValue()));
    return &c;
}

ast::Expression* ExprContext::analyzeImpl(ast::MoveExpr& expr) {
    if (!analyzeValue(expr.value())) {
        return nullptr;
    }
    bool success = true;
    auto type = expr.value()->type();
    auto& lifetime = type->lifetimeMetadata();
    std::optional op = lifetime.moveOrCopyConstructor();
    using enum SMFKind;
    auto ctorKind = *op == lifetime.copyConstructor() ? CopyConstructor :
                                                        MoveConstructor;
    if (!type->isComplete()) {
        ctx.badExpr(&expr, BadExpr::MoveExprIncompleteType);
        success = false;
    }
    if (type.isConst()) {
        ctx.badExpr(&expr, BadExpr::MoveExprConst);
        success = false;
    }
    if (op->isDeleted()) {
        ctx.badExpr(&expr, BadExpr::MoveExprImmovable);
        success = false;
    }
    if (!success) {
        return nullptr;
    }
    if (*op == lifetime.copyConstructor()) {
        /// Warning
        ctx.badExpr(&expr, BadExpr::MoveExprCopies);
    }
    if (expr.value()->isRValue()) {
        /// Warning
        ctx.badExpr(&expr, BadExpr::MoveExprRValue);
        /// Here we don't want to generate code for the move operation
        op = std::nullopt;
    }
    if (!op) {
        expr.decorateMove(expr.value()->object(), op, ctorKind);
        return &expr;
    }
    auto* tmp = sym.temporary(&expr, expr.value()->type());
    expr.decorateMove(tmp, op, ctorKind);
    if (!currentCleanupStack().push(tmp, ctx)) {
        return nullptr;
    }
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::UniqueExpr& expr) {
    if (expr.value() && !analyze(expr.value())) {
        return nullptr;
    }
    auto cat = expr.value() ? expr.value()->entityCategory() :
                              EntityCategory::Type;
    switch (cat) {
    case EntityCategory::Value: {
        SC_ASSERT(expr.value(), "");
        if (!expr.value()->isRValue()) {
            ctx.badExpr(&expr, UniqueExprNoRValue);
            return nullptr;
        }
        /// We pop the top level dtor because unique ptr extends the lifetime of
        /// the object
        currentCleanupStack().pop(expr.value());
        auto* type = sym.uniquePointer(expr.value()->type());
        expr.decorateValue(sym.temporary(&expr, type), RValue);
        if (!currentCleanupStack().push(expr.object(), ctx)) {
            return nullptr;
        }
        return &expr;
    }
    case EntityCategory::Type: {
        if (!isa<ast::DereferenceExpression>(expr.parent())) {
            ctx.badExpr(&expr, GenericBadExpr);
            return nullptr;
        }
        if (!expr.value()) {
            expr.decorateType(
                sym.typeDeductionQualifier(ReferenceKind::UniquePointer,
                                           expr.mutability()));
        }
        else {
            expr.decorateType(cast<Type*>(expr.value()->entity()));
        }
        return &expr;
    }
    default:
        SC_UNREACHABLE();
    }
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
    convert(Implicit, expr.argument(0), sym.S64(), RValue,
            currentCleanupStack(), ctx);
    auto mutability = expr.callee()->type().mutability();
    auto elemType = QualType(arrayType->elementType(), mutability);
    expr.decorateValue(sym.temporary(&expr, elemType),
                       expr.callee()->valueCategory());
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::SubscriptSlice& expr) {
    auto* arrayType = analyzeSubscriptCommon(expr);
    if (!arrayType) {
        return nullptr;
    }
    convert(Implicit, expr.lower(), sym.S64(), RValue, currentCleanupStack(),
            ctx);
    convert(Implicit, expr.upper(), sym.S64(), RValue, currentCleanupStack(),
            ctx);
    auto dynArrayType = sym.arrayType(arrayType->elementType());
    expr.decorateValue(sym.temporary(&expr, dynArrayType),
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
    expr.decorateValue(sym.temporary(&expr, getQualType(resultType)),
                       refToLValue(resultType));
    return &expr;
}

static void convertArgsAndPopCleanups(auto const& arguments,
                                      auto const& conversions,
                                      CleanupStack& cleanups,
                                      AnalysisContext& ctx) {
    for (auto [arg, conv]: ranges::views::zip(arguments, conversions)) {
        /// The called function will clean up the arguments
        auto* converted = insertConversion(arg, conv, cleanups, ctx);
        if (converted && converted->isRValue()) {
            cleanups.erase(converted);
        }
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::FunctionCall& fc) {
    auto orKind = ORKind::FreeFunction;
    bool success = analyze(fc.callee());
    success &= analyzeValues(fc.arguments());
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
        [[maybe_unused]] auto* converted =
            convert(Reinterpret, arg, genExpr->type(), genExpr->valueCategory(),
                    currentCleanupStack(), ctx);
        return fc.replace(fc.extractArgument(0));
    }
    /// if our object is a type, then we rewrite the AST so we end up with just
    /// a conversion or construct expr
    if (auto* type = dyncast<ObjectType const*>(fc.callee()->entity())) {
        /// Kinda ugly that we need this check here. Maybe we can refactor that
        /// in the future
        if (fc.arguments().size() == 1) {
            auto* arg = fc.replace(fc.argument(0)->extractFromParent());
            return convert(Explicit, arg, QualType::Mut(type), RValue,
                           currentCleanupStack(), ctx);
        }
        else {
            return constructInplace(ConversionKind::Explicit, &fc, type,
                                    fc.arguments() | ToSmallVector<>,
                                    currentCleanupStack(), ctx);
        }
    }
    /// Make sure we have an overload set as our called object
    auto* overloadSet = dyncast<OverloadSet*>(fc.callee()->entity());
    if (!overloadSet) {
        ctx.badExpr(&fc, ObjectNotCallable);
        return nullptr;
    }
    /// Perform overload resolution
    auto result = performOverloadResolution(&fc, *overloadSet,
                                            fc.arguments() | ToSmallVector<>,
                                            orKind);
    if (result.error) {
        result.error->setSourceRange(fc.sourceRange());
        ctx.issueHandler().push(std::move(result.error));
        return nullptr;
    }
    auto* function = result.function;
    /// Cannot explicitly call special member functions
    if (isNewMoveDelete(*function)) {
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
    fc.decorateCall(sym.temporary(&fc, type), valueCat, type, function);
    convertArgsAndPopCleanups(fc.arguments(), result.conversions,
                              currentCleanupStack(), ctx);
    if (valueCat == RValue && !currentCleanupStack().push(fc.object(), ctx)) {
        return nullptr;
    }
    return &fc;
}

ast::Expression* ExprContext::analyzeImpl(ast::ListExpression& list) {
    bool success = true;
    for (auto* expr: list.elements()) {
        expr = analyze(expr);
        success &= !!expr;
        currentCleanupStack().pop(expr);
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
            bool const succ = convert(Implicit, expr, commonType, RValue,
                                      currentCleanupStack(), ctx);
            SC_ASSERT(succ, "Conversion failed despite common type");
        }
        auto* arrayType =
            sym.arrayType(commonType.get(), list.elements().size());
        list.decorateValue(sym.temporary(&list, arrayType), RValue);
        if (!currentCleanupStack().push(list.object(), ctx)) {
            return nullptr;
        }
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
            auto* value = cast<IntValue const*>(countExpr->constantValue());
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
    case EntityCategory::Namespace:
        ctx.issue<BadSymRef>(&list, EntityCategory::Value);
        return nullptr;
    case EntityCategory::Indeterminate:
        SC_ASSERT(
            !ctx.issueHandler().empty(),
            "There must be an issue if we have an indeterminate entity here");
    }
    SC_UNREACHABLE();
}

ast::Expression* ExprContext::analyzeImpl(ast::NontrivAssignExpr& expr) {
    bool argsAnalyzed = true;
    argsAnalyzed &= !!analyzeValue(expr.dest());
    argsAnalyzed &= !!analyzeValue(expr.source());
    if (!argsAnalyzed) {
        return nullptr;
    }
    if (expr.dest()->type().get() != expr.source()->type().get()) {
        if (!convert(Implicit, expr.source(), expr.dest()->type(), RValue,
                     currentCleanupStack(), ctx))
            return nullptr;
    }
    auto& md = expr.dest()->type()->lifetimeMetadata();
    auto copy = md.copyConstructor();
    auto move = md.moveConstructor();
    auto destroy = md.destructor();
    if (destroy.isDeleted()) {
        ctx.issue<BadExpr>(&expr, CannotAssignIndestructibleType);
        return nullptr;
    }
    if (expr.source()->isLValue()) {
        if (copy.isDeleted()) {
            ctx.issue<BadExpr>(&expr, CannotAssignUncopyableType);
            return nullptr;
        }
        expr.decorateAssign(sym.temporary(&expr, sym.Void()),
                            SMFKind::CopyConstructor,
                            /* checkSelfAssign = */ true);
    }
    else {
        auto resolvedOp = !move.isDeleted() ? move : copy;
        if (resolvedOp.isDeleted()) {
            ctx.issue<BadExpr>(&expr, CannotAssignUncopyableType);
            return nullptr;
        }
        auto ctor = resolvedOp == move ? SMFKind::MoveConstructor :
                                         SMFKind::CopyConstructor;
        expr.decorateAssign(sym.temporary(&expr, sym.Void()), ctor,
                            /* checkSelfAssign = */ false);
    }
    return &expr;
}

/// \Returns the target value category given the value category conversion \p
/// conv
static ValueCategory targetValueCat(ValueCatConversion conv) {
    using enum ValueCatConversion;
    switch (conv) {
    case LValueToRValue:
        return RValue;
    case MaterializeTemporary:
        return LValue;
    }
    SC_UNREACHABLE();
}

/// \Returns a temporary if the conversion \p conv creates a new object or the
/// \p original otherwise
static Object* convertedObject(ast::ASTNode* astNode, Entity* original,
                               ValueCatConversion conv, SymbolTable& sym) {
    auto* obj = cast<Object*>(original);
    if (conv == ValueCatConversion::LValueToRValue) {
        return sym.temporary(astNode, obj->getQualType());
    }
    return obj;
}

ast::Expression* ExprContext::analyzeImpl(ast::ValueCatConvExpr& vcConv) {
    auto* expr = vcConv.expression();
    if (!analyzeValue(expr)) {
        return nullptr;
    }
    auto valueCat = targetValueCat(vcConv.conversion());
    /// This function is very hacky. To avoid allocations we don't create new
    /// "objects" (entities) for MutConv and ValueCatConv expressions and thus
    /// can't propagate mutability easily. This is why we have the explicit type
    /// argument to `decorateValue`
    auto* obj = convertedObject(expr, expr->entity(), vcConv.conversion(), sym);
    auto mutCorrectedType = obj->getQualType().to(expr->type().mutability());
    vcConv.decorateValue(obj, valueCat, mutCorrectedType);
    vcConv.setConstantValue(clone(expr->constantValue()));
    return &vcConv;
}

static QualType mutConvType(QualType original, MutConversion conv) {
    using enum MutConversion;
    switch (conv) {
    case MutToConst:
        return QualType::Const(original.get());
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::MutConvExpr& mutConv) {
    auto* expr = mutConv.expression();
    if (!analyzeValue(expr)) {
        return nullptr;
    }
    mutConv.decorateValue(expr->entity(), expr->valueCategory(),
                          mutConvType(expr->type(), mutConv.conversion()));
    mutConv.setConstantValue(clone(expr->constantValue()));
    return &mutConv;
}

static QualType ptrBase(ObjectType const* from) {
    return cast<PointerType const*>(from)->base();
}

///
static ArrayType const* arrayFixedToDyn(ObjectType const* array,
                                        SymbolTable& sym) {
    auto* A = cast<ArrayType const*>(array);
    return sym.arrayType(A->elementType());
}

/// \Returns the kind of pointer type of \p ptrOrRef (i.e. raw or unique
/// pointer) but pointing to \p pointee
static PointerType const* repoint(PointerType const* ptr, QualType pointee,
                                  SymbolTable& sym) {
    // clang-format off
    return SC_MATCH_R (PointerType const*, *ptr) {
        [&](RawPtrType const&) {
            return sym.pointer(pointee);
        },
        [&](UniquePtrType const&) {
            return sym.uniquePointer(pointee);
        },
        [&](Type const&) { SC_UNREACHABLE(); }
    }; // clang-format on
}

ObjectType const* computeConvertedObjType(ObjectTypeConversion conv,
                                          ObjectType const* from,
                                          SymbolTable& sym) {
    using enum ObjectTypeConversion;
    switch (conv) {
    case UniqueToRawPtr: {
        return sym.pointer(ptrBase(from));
    }
    case Reinterpret_ValuePtr_ToByteArray: {
        auto base = ptrBase(from);
        return repoint(cast<PointerType const*>(from),
                       QualType(sym.arrayType(sym.Byte(), base->size()),
                                base.mutability()),
                       sym);
    }
    case Reinterpret_ValueRef_ToByteArray: {
        return sym.arrayType(sym.Byte(), from->size());
    }
    case ArrayPtr_FixedToDynamic: {
        auto base = ptrBase(from);
        return repoint(cast<PointerType const*>(from),
                       QualType(arrayFixedToDyn(base.get(), sym),
                                base.mutability()),
                       sym);
    }
    case ArrayRef_FixedToDynamic:
        return arrayFixedToDyn(from, sym);
    case IntTruncTo8:
        return sym.intType(8, cast<IntType const*>(from)->signedness());
    case IntTruncTo16:
        return sym.intType(16, cast<IntType const*>(from)->signedness());
    case IntTruncTo32:
        return sym.intType(32, cast<IntType const*>(from)->signedness());
    case SignedWidenTo16:
        return sym.S16();
    case SignedWidenTo32:
        return sym.S32();
    case SignedWidenTo64:
        return sym.S64();
    case UnsignedWidenTo16:
        return sym.U16();
    case UnsignedWidenTo32:
        return sym.U32();
    case UnsignedWidenTo64:
        return sym.U64();
    case FloatTruncTo32:
        return sym.Float();
    case FloatWidenTo64:
        return sym.Double();
    case SignedToUnsigned:
        return sym.intType(cast<IntType const*>(from)->bitwidth(),
                           Signedness::Unsigned);
    case UnsignedToSigned:
        return sym.intType(cast<IntType const*>(from)->bitwidth(),
                           Signedness::Signed);
    case SignedToFloat32:
        return sym.Float();
    case SignedToFloat64:
        return sym.Double();
    case UnsignedToFloat32:
        return sym.Float();
    case UnsignedToFloat64:
        return sym.Double();
    case FloatToSigned8:
        return sym.S8();
    case FloatToSigned16:
        return sym.S16();
    case FloatToSigned32:
        return sym.S32();
    case FloatToSigned64:
        return sym.S64();
    case FloatToUnsigned8:
        return sym.U8();
    case FloatToUnsigned16:
        return sym.U16();
    case FloatToUnsigned32:
        return sym.U32();
    case FloatToUnsigned64:
        return sym.U64();
    case IntToByte:
        return sym.Byte();
    case ByteToSigned:
        return sym.S8();
    case ByteToUnsigned:
        return sym.U8();
    default:
        SC_UNREACHABLE();
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::ObjTypeConvExpr& conv) {
    auto* expr = conv.expression();
    if (!analyzeValue(expr)) {
        return nullptr;
    }
    auto* type = conv.targetType() ?
                     conv.targetType() :
                     computeConvertedObjType(conv.conversion(),
                                             conv.expression()->type().get(),
                                             sym);
    auto* object = sym.temporary(&conv, type);
    using enum ObjectTypeConversion;
    switch (conv.conversion()) {
    case ArrayPtr_FixedToDynamic:
    case Reinterpret_ValuePtr:
    case Reinterpret_ValuePtr_ToByteArray:
    case Reinterpret_ValuePtr_FromByteArray:
    case Reinterpret_DynArrayPtr_ToByte:
    case Reinterpret_DynArrayPtr_FromByte:
        /// May convert non-trivial objects
        currentCleanupStack().pop(conv.expression());
        if (!currentCleanupStack().push(object, ctx)) {
            return nullptr;
        }
        break;
    case NullptrToUniquePtr:
        /// Creates a new non-trivial object
        if (!currentCleanupStack().push(object, ctx)) {
            return nullptr;
        }
        break;
    default:
        break;
    }
    conv.decorateValue(object, expr->valueCategory());
    conv.setConstantValue(
        evalConversion(conv.conversion(), expr->constantValue()));
    return &conv;
}

ast::Expression* ExprContext::analyzeImpl(ast::TrivDefConstructExpr& expr) {
    expr.decorateConstruct(sym.temporary(&expr, expr.constructedType()));
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::TrivCopyConstructExpr& expr) {
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    auto* type = expr.constructedType();
    expr.decorateConstruct(sym.temporary(&expr, type));
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::TrivAggrConstructExpr& expr) {
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    /// Can `cast` because only structs are aggregates
    auto* structType = cast<StructType const*>(expr.constructedType());
    if (expr.arguments().size() != structType->members().size()) {
        ctx.badExpr(&expr, CannotConstructType);
        return nullptr;
    }
    bool success = true;
    for (auto [arg, type]:
         ranges::views::zip(expr.arguments(), structType->members()))
    {
        success &= !!convert(Implicit, arg, getQualType(type), RValue,
                             currentCleanupStack(), ctx);
    }
    if (!success) {
        return nullptr;
    }
    expr.decorateConstruct(sym.temporary(&expr, structType));
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::NontrivConstructExpr& expr) {
    auto* type = expr.constructedType();
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    if (type->constructors().empty()) {
        ctx.badExpr(&expr, CannotConstructType);
        return nullptr;
    }
    /// Only used for overload resolution. Will be deallocated when this
    /// function returns
    auto uninitTmp = allocate<ast::UninitTemporary>(SourceRange{});
    uninitTmp->decorateValue(sym.temporary(&expr, type), LValue);
    auto args = concat(single(uninitTmp.get()), expr.arguments()) |
                ToSmallVector<>;
    auto orResult =
        performOverloadResolution(&expr, type->constructors(), args);
    if (orResult.error) {
        ctx.issueHandler().push(std::move(orResult.error));
        return nullptr;
    }
    SC_ASSERT(orResult.conversions.front().isNoop(), "");
    convertArgsAndPopCleanups(expr.arguments(), orResult.conversions | drop(1),
                              currentCleanupStack(), ctx);
    auto* obj = sym.temporary(&expr, type);
    expr.decorateConstruct(obj, orResult.function);
    if (!currentCleanupStack().push(obj, ctx)) {
        return nullptr;
    }
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(
    ast::NontrivInlineConstructExpr& expr) {
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    if (auto* type = dyncast<ArrayType const*>(expr.constructedType())) {
        auto operation = [&]() -> std::optional<SMFKind> {
            auto& elemLifetime = type->elementType()->lifetimeMetadata();
            switch (expr.arguments().size()) {
            case 0: { // Default construction
                if (elemLifetime.defaultConstructor().isDeleted()) {
                    ctx.badExpr(&expr, CannotConstructType);
                    return std::nullopt;
                }
                return SMFKind::DefaultConstructor;
            }
            case 1: { // Copy or move construction
                auto op = elemLifetime.moveOrCopyConstructor();
                if (op == elemLifetime.moveConstructor()) {
                    return SMFKind::MoveConstructor;
                }
                if (op == elemLifetime.copyConstructor()) {
                    return SMFKind::CopyConstructor;
                }
                ctx.badExpr(&expr, CannotConstructType);
                return std::nullopt;
            }
            default:
                ctx.badExpr(&expr, CannotConstructType);
                return std::nullopt;
            }
        }();
        if (!operation) {
            return nullptr;
        }
        auto* obj = sym.temporary(&expr, type);
        expr.decorateConstruct(obj, *operation);
        if (!currentCleanupStack().push(obj, ctx)) {
            return nullptr;
        }
        return &expr;
    }
    if (auto* type = dyncast<UniquePtrType const*>(expr.constructedType())) {
        switch (expr.arguments().size()) {
        case 0: { // Default construction
            auto* obj = sym.temporary(&expr, type);
            expr.decorateConstruct(obj);
            if (!currentCleanupStack().push(obj, ctx)) {
                return nullptr;
            }
            return &expr;
        }
        default:
            ctx.badExpr(&expr, CannotConstructType);
            return nullptr;
        }
    }
    else {
        ctx.badExpr(&expr, CannotConstructType);
        return nullptr;
    }
}

ast::Expression* ExprContext::analyzeImpl(ast::NontrivAggrConstructExpr& expr) {
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    auto* type = expr.constructedType();
    if (expr.arguments().size() != type->members().size()) {
        ctx.badExpr(&expr, CannotConstructType);
        return nullptr;
    }
    bool success = true;
    for (auto [arg, argType]:
         ranges::views::zip(expr.arguments(), type->members()))
    {
        auto* conv = convert(Implicit, arg,
                             QualType::Mut(cast<ObjectType const*>(argType)),
                             RValue, currentCleanupStack(), ctx);
        if (!conv) {
            success = false;
            continue;
        }
        currentCleanupStack().pop(conv);
    }
    if (!success) {
        return nullptr;
    }
    auto* obj = sym.temporary(&expr, type);
    if (!currentCleanupStack().push(obj, ctx)) {
        return nullptr;
    }
    expr.decorateConstruct(obj);
    return &expr;
}

ast::Expression* ExprContext::analyzeImpl(ast::DynArrayConstructExpr& expr) {
    auto* type = expr.constructedType();
    SC_EXPECT(type->isDynamic());
    if (!analyzeValues(expr.arguments())) {
        return nullptr;
    }
    if (!isa<ast::UniqueExpr>(expr.parent())) {
        ctx.issue<BadExpr>(&expr, DynArrayConstrAutoStorage);
        return nullptr;
    }
    switch (expr.arguments().size()) {
    case 1: {
        auto* arg = expr.argument(0);
        if (auto intConv = computeConversion(Implicit, arg, sym.Int(), RValue))
        {
            arg = insertConversion(arg, intConv.value(), currentCleanupStack(),
                                   ctx);
            auto insert = [&](auto constr) {
                return expr.setElementConstruction(std::move(constr));
            };
            auto* constr = constructInplace(ConversionKind::Implicit, &expr,
                                            insert, type->elementType(), {},
                                            currentCleanupStack(), ctx)
                               .valueOr(nullptr);
            if (!constr) {
                return nullptr;
            }
            currentCleanupStack().pop(expr.elementConstruction());
            expr.decorateConstruct(sym.temporary(&expr, type));
            if (!currentCleanupStack().push(expr.object(), ctx)) {
                return nullptr;
            }
            return &expr;
        }
        ctx.issue<BadExpr>(&expr, DynArrayConstrBadArgs);
        return nullptr;
    }
    default:
        ctx.issue<BadExpr>(&expr, DynArrayConstrBadArgs);
        return nullptr;
    }
}

void ExprContext::dereferencePointer(ast::Expression* expr) {
    if (!expr || !expr->isDecorated()) {
        return;
    }
    auto* type = expr->type().get();
    if (!isa<PointerType>(type)) {
        return;
    }
    SC_EXPECT(expr->isValue());
    auto* parent = expr->parent();
    SC_EXPECT(parent);
    size_t index = expr->indexInParent();
    auto deref = allocate<ast::DereferenceExpression>(expr->extractFromParent(),
                                                      Mutability::Const,
                                                      expr->sourceRange());
    bool result = analyzeValue(deref.get());
    SC_ASSERT(result, "How can a pointer dereference fail?");
    parent->setChild(index, std::move(deref));
}

Type const* ExprContext::getReturnType(Function* function) {
    if (function->returnType()) {
        return function->returnType();
    }
    sym.withScopeCurrent(function->parent(), [&] {
        auto* def = cast<ast::FunctionDefinition*>(function->astNode());
        analyzeStatement(ctx, def);
    });
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
