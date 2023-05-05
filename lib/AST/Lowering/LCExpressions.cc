#include "AST/Lowering/LoweringContext.h"

#include <svm/Builtin.h>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

static bool isArray(Expression const* expr) {
    return expr->type() && isa<sema::ArrayType>(expr->type()->base());
}

static bool isIntType(size_t width, ir::Type const* type) {
    return cast<ir::IntegralType const*>(type)->bitWidth() == 1;
}

/// MARK: getValue() Implementation

ir::Value* LoweringContext::getValue(Expression const* expr) {
    return visit(*expr,
                 [this](auto const& expr) { return getValueImpl(expr); });
}

ir::Value* LoweringContext::getValueImpl(Identifier const& id) {
    return add<ir::Load>(getAddressImpl(id),
                         mapType(id.type()->base()),
                         id.value());
}

ir::Value* LoweringContext::getValueImpl(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        return intConstant(lit.value<LiteralKind::Integer>());
    case LiteralKind::Boolean:
        return intConstant(lit.value<LiteralKind::Boolean>());
    case LiteralKind::FloatingPoint:
        return floatConstant(lit.value<LiteralKind::FloatingPoint>());
    case LiteralKind::This: {
        return add<ir::Load>(getAddress(&lit),
                             mapType(lit.type()->base()),
                             "this");
    }
    case LiteralKind::String:
        SC_DEBUGFAIL();
    }
}

ir::Value* LoweringContext::getValueImpl(UnaryPrefixExpression const& expr) {
    switch (expr.operation()) {
        using enum UnaryPrefixOperator;
    case Increment:
        [[fallthrough]];
    case Decrement: {
        ir::Value* const operandAddress = getAddress(expr.operand());
        SC_ASSERT(isa<ir::PointerType>(operandAddress->type()),
                  "We need a pointer to store to");
        ir::Type const* arithmeticType =
            mapType(expr.operand()->type()->base());
        ir::Value* operandValue =
            add<ir::Load>(operandAddress,
                          arithmeticType,
                          utl::strcat(expr.operation(), ".operand"));
        auto* newValue =
            add<ir::ArithmeticInst>(operandValue,
                                    intConstant(1, 64),
                                    expr.operation() == Increment ?
                                        ir::ArithmeticOperation::Add :
                                        ir::ArithmeticOperation::Sub,
                                    utl::strcat(expr.operation(), ".result"));
        add<ir::Store>(operandAddress, newValue);
        return newValue;
    }

    case ast::UnaryPrefixOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryPrefixOperator::Negation: {
        auto* operand  = getValue(expr.operand());
        auto operation = isa<ir::IntegralType>(operand->type()) ?
                             ir::ArithmeticOperation::Sub :
                             ir::ArithmeticOperation::FSub;
        return add<ir::ArithmeticInst>(constant(0, operand->type()),
                                       operand,
                                       operation,
                                       "negated");
    }

    default:
        return add<ir::UnaryArithmeticInst>(getValue(expr.operand()),
                                            mapUnaryOp(expr.operation()),
                                            "expr.result");
    }
}

ir::Value* LoweringContext::getValueImpl(BinaryExpression const& expr) {
    auto* structType =
        dyncast<sema::StructureType const*>(expr.lhs()->type()->base());

    switch (expr.operation()) {
        using enum BinaryOperator;
    case Multiplication:
        [[fallthrough]];
    case Division:
        [[fallthrough]];
    case Remainder:
        [[fallthrough]];
    case Addition:
        [[fallthrough]];
    case Subtraction:
        [[fallthrough]];
    case LeftShift:
        [[fallthrough]];
    case RightShift:
        [[fallthrough]];
    case BitwiseAnd:
        [[fallthrough]];
    case BitwiseXOr:
        [[fallthrough]];
    case BitwiseOr: {
        ir::Value* const lhs = getValue(expr.lhs());
        ir::Value* const rhs = getValue(expr.rhs());
        auto* type           = lhs->type();
        if (expr.operation() != LeftShift && expr.operation() != RightShift) {
            SC_ASSERT(lhs->type() == rhs->type(),
                      "Need same types to do arithmetic");
            SC_ASSERT(isa<ir::ArithmeticType>(type),
                      "Need arithmetic type to do arithmetic");
        }
        else {
            SC_ASSERT(isa<ir::IntegralType>(lhs->type()),
                      "Need integral type for shift");
            SC_ASSERT(isa<ir::IntegralType>(rhs->type()),
                      "Need integral type for shift");
        }
        auto operation = mapArithmeticOp(structType, expr.operation());
        return add<ir::ArithmeticInst>(lhs, rhs, operation, "expr.result");
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue(expr.lhs());
        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
        auto* startBlock = currentBlock;
        auto* rhsBlock   = newBlock("log.rhs");
        auto* endBlock   = newBlock("log.end");
        if (expr.operation() == LogicalAnd) {
            add<ir::Branch>(lhs, rhsBlock, endBlock);
        }
        else {
            add<ir::Branch>(lhs, endBlock, rhsBlock);
        }

        add(rhsBlock);
        auto* rhs = getValue(expr.rhs());
        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
        add<ir::Goto>(endBlock);
        add(endBlock);

        return [&] {
            if (expr.operation() == LogicalAnd) {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(0, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.and.value");
            }
            else {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, intConstant(1, 1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.or.value");
            }
        }();
    }

    case Less:
        [[fallthrough]];
    case LessEq:
        [[fallthrough]];
    case Greater:
        [[fallthrough]];
    case GreaterEq:
        [[fallthrough]];
    case Equals:
        [[fallthrough]];
    case NotEquals: {
        return add<ir::CompareInst>(getValue(expr.lhs()),
                                    getValue(expr.rhs()),
                                    mapCompareMode(structType),
                                    mapCompareOp(expr.operation()),
                                    "cmp.result");
    }

    case Comma:
        return getValue(expr.lhs()), getValue(expr.rhs());

    case Assignment:
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
        auto* lhs = getAddress(expr.lhs());
        auto* rhs = getValue(expr.rhs());
        if (expr.operation() != Assignment) {
            auto* structType =
                cast<sema::StructureType const*>(expr.lhs()->type()->base());
            auto* lhsValue =
                add<ir::Load>(lhs, mapType(structType), "lhs.value");
            rhs =
                add<ir::ArithmeticInst>(lhsValue,
                                        rhs,
                                        mapArithmeticAssignOp(structType,
                                                              expr.operation()),
                                        "expr.result");
        }
        return add<ir::Store>(lhs, rhs);
    }
    case BinaryOperator::_count:
        SC_UNREACHABLE();
    }
}

ir::Value* LoweringContext::getValueImpl(MemberAccess const& expr) {
    if (auto itr = valueMap.find(expr.member()->entity());
        itr != valueMap.end())
    {
        return itr->second;
    }
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(expr.object()->type()->base()))
    {
        SC_ASSERT(expr.member()->value() == "count", "What else?");
        auto* addr = getAddress(expr.object());
        return getArrayCount(addr);
    }
    if (hasAddress(&expr)) {
        return add<ir::Load>(getAddressImpl(expr),
                             mapType(expr.type()),
                             "member.access");
    }
    auto* base       = getValue(expr.object());
    auto* accessedId = cast<Identifier const*>(expr.member());
    auto* var        = cast<sema::Variable const*>(accessedId->entity());
    return add<ir::ExtractValue>(base,
                                 std::array{ var->index() },
                                 "member.access");
}

ir::Value* LoweringContext::getValueImpl(ReferenceExpression const& expr) {
    SC_ASSERT(expr.referred()->type()->isReference() ||
                  expr.referred()->isLValue(), // Maybe defer this assertion to
                                               // getAddress()?
              "");
    return getAddress(expr.referred());
}

ir::Value* LoweringContext::getValueImpl(UniqueExpression const& expr) {
    SC_DEBUGFAIL();
}

ir::Value* LoweringContext::getValueImpl(Conditional const& condExpr) {
    auto* cond      = getValue(condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock  = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto* thenVal = getValue(condExpr.thenExpr());
    thenBlock     = currentBlock; /// Nested `?:` operands etc. may have changed
                                  /// `currentBlock`
    add<ir::Goto>(endBlock);

    /// Generate else block.
    add(elseBlock);
    auto* elseVal = getValue(condExpr.elseExpr());
    elseBlock     = currentBlock;
    add<ir::Goto>(endBlock);

    /// Generate end block.
    add(endBlock);
    return add<ir::Phi>(std::array<ir::PhiMapping,
                                   2>{ ir::PhiMapping{ thenBlock, thenVal },
                                       ir::PhiMapping{ elseBlock, elseVal } },
                        "cond.result");
}

ir::Value* LoweringContext::getValueImpl(FunctionCall const& expr) {
    auto* call = genCall(&expr);
    if (expr.type()->isReference()) {
        return add<ir::Load>(call, mapType(expr.type()->base()), "call.value");
    }
    return call;
}

ir::Value* LoweringContext::getValueImpl(Subscript const& expr) {
    auto* address = getAddress(&expr);
    return add<ir::Load>(address, mapType(expr.type()->base()), "[].value");
}

ir::Value* LoweringContext::getValueImpl(Conversion const& conv) {
    auto* expr = conv.expression();
    if (conv.type()->isExplicitReference()) {
        return getAddress(expr);
    }
    return getValue(expr);
}

ir::Value* LoweringContext::getValueImpl(ListExpression const& list) {
    SC_DEBUGFAIL();
}

/// MARK: getAddress() Implementation

ir::Value* LoweringContext::getAddress(Expression const* expr) {
    SC_ASSERT(hasAddress(expr), "");
    return visit(*expr,
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* LoweringContext::getAddressImpl(Literal const& lit) {
    SC_ASSERT(lit.kind() == LiteralKind::This, "");
    return variableAddressMap[lit.entity()];
}

ir::Value* LoweringContext::getAddressImpl(Identifier const& id) {
    auto* address = variableAddressMap[id.entity()];
    SC_ASSERT(address, "Undeclared identifier");
    SC_ASSERT(id.type()->isImplicitReference() || id.isLValue(),
              "Just to be safe");
    if (id.type()->isImplicitReference()) {
        auto* refType = isa<sema::ArrayType>(id.type()->base()) ?
                            arrayViewType :
                            ctx.pointerType();
        return add<ir::Load>(address,
                             refType,
                             utl::strcat(id.value(), ".value"));
    }
    return address;
}

ir::Value* LoweringContext::getAddressImpl(MemberAccess const& expr) {
    auto* gep     = getAddressLocImpl(expr);
    auto* memType = expr.member()->type();
    if (!memType->isReference()) {
        return gep;
    }
    return add<ir::Load>(gep,
                         ctx.pointerType(),
                         utl::strcat(gep->name(), ".value"));
}

ir::Value* LoweringContext::getAddressImpl(FunctionCall const& expr) {
    SC_ASSERT(expr.type()->isReference(),
              "Can't be l-value so we expect a reference");
    return genCall(&expr);
}

ir::Value* LoweringContext::getAddressImpl(Subscript const& expr) {
    auto* arrayType =
        cast<sema::ArrayType const*>(expr.object()->type()->base());
    auto* elemType  = mapType(arrayType->elementType());
    auto* arrayRef  = getAddress(expr.object());
    auto* arrayData = getArrayAddr(arrayRef);
    auto* index     = getValue(expr.arguments().front());
    return add<ir::GetElementPointer>(elemType,
                                      arrayData,
                                      index,
                                      std::initializer_list<size_t>{},
                                      "elem.ptr");
}

ir::Value* LoweringContext::getAddressImpl(ReferenceExpression const& expr) {
    return getAddressLocation(expr.referred());
}

ir::Value* LoweringContext::getAddressImpl(Conversion const& conv) {
    if (conv.type()->isExplicitReference()) {
        return getAddressLocation(conv.expression());
    }
    return getAddress(conv.expression());
}

static bool evalConstant(Expression const* expr, utl::vector<u8>& dest) {
    auto* lit = dyncast<Literal const*>(expr);
    if (!lit) {
        return false;
    }
    auto value =
        std::get<static_cast<size_t>(LiteralKind::Integer)>(lit->value());
    size_t const elemSize = lit->type()->size();
    auto* data            = reinterpret_cast<u8 const*>(value.limbs().data());
    for (auto* end = data + elemSize; data < end; ++data) {
        dest.push_back(*data);
    }
    return true;
}

bool LoweringContext::genStaticListData(ListExpression const& list,
                                        ir::Alloca* dest) {
    auto* type     = cast<sema::ArrayType const*>(list.type()->base());
    auto* elemType = type->elementType();
    utl::small_vector<u8> data;
    data.reserve(type->size());
    for (auto* expr: list.elements()) {
        SC_ASSERT(elemType == expr->type()->base(), "Invalid type");
        if (!evalConstant(expr, data)) {
            return false;
        }
    }
    auto constData =
        allocate<ir::ConstantData>(ctx,
                                   ctx.arrayType(mapType(elemType),
                                                 list.elements().size()),
                                   std::move(data),
                                   "array");
    auto* source = constData.get();
    mod.addConstantData(std::move(constData));
    auto* memcpy = getFunction(
        symbolTable.builtinFunction(static_cast<size_t>(svm::Builtin::memcpy)));
    add<ir::Call>(memcpy,
                  std::array<ir::Value*, 3>{
                      dest,
                      source,
                      intConstant(list.elements().size() * elemType->size(),
                                  64) },
                  std::string{});
    return true;
}

void LoweringContext::genListDataSlowPath(ListExpression const& list,
                                          ir::Alloca* dest) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type()->base());
    auto* elemType  = mapType(arrayType->elementType());
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* gep = add<ir::GetElementPointer>(elemType,
                                               dest,
                                               intConstant(index, 32),
                                               std::initializer_list<size_t>{},
                                               "elem.ptr");
        add<ir::Store>(gep, getValue(elem));
    }
}

ir::Value* LoweringContext::getAddressImpl(ListExpression const& list) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type()->base());
    auto* elemType  = mapType(arrayType->elementType());
    auto* count     = intConstant(list.elements().size(), 64);
    auto* array     = new ir::Alloca(ctx, count, elemType, "array");
    allocas.push_back(array);
    valueMap.insert({ arrayType->countVariable(), count });
    if (genStaticListData(list, array)) {
        return makeArrayRef(array, count);
    }
    genListDataSlowPath(list, array);
    return makeArrayRef(array, count);
}

ir::Value* LoweringContext::getAddressLocation(Expression const* expr) {
    return visit(*expr,
                 [this](auto const& expr) { return getAddressLocImpl(expr); });
}

ir::Value* LoweringContext::getAddressLocImpl(Identifier const& id) {
    SC_ASSERT(id.isLValue() && id.type()->isReference(), "");
    auto* address = variableAddressMap[id.entity()];
    SC_ASSERT(address, "Undeclared identifier");
    return address;
}

ir::Value* LoweringContext::getAddressLocImpl(MemberAccess const& expr) {
    auto* base       = getAddress(expr.object());
    auto* accessedId = cast<Identifier const*>(expr.member());
    auto* var        = cast<sema::Variable const*>(accessedId->entity());
    auto* type       = mapType(expr.object()->type()->base());
    return add<ir::GetElementPointer>(type,
                                      base,
                                      intConstant(0, 64),
                                      std::array{ var->index() },
                                      "mem.ptr");
}
