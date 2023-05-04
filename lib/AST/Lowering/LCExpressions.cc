#include "AST/Lowering/LoweringContext.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

ir::Value* LoweringContext::getValue(Expression const* expr) {
    //    return visit(*expr, [this](auto const& expr) { return
    //    getValueImpl(expr); });
}
//
// ir::Value* LoweringContext::getValueImpl(Identifier const& id) {
//    auto* address = getAddress(id);
//    return loadAddress(address, mapType(id.type()->base()), id.value());
//}
//
// ir::Value* LoweringContext::getValueImpl(Literal const& lit) {
//    switch (lit.kind()) {
//    case LiteralKind::Integer:
//        return irCtx.integralConstant(lit.value<LiteralKind::Integer>());
//    case LiteralKind::Boolean:
//        return irCtx.integralConstant(lit.value<LiteralKind::Boolean>());
//    case LiteralKind::FloatingPoint:
//        return irCtx.floatConstant(lit.value<LiteralKind::FloatingPoint>(),
//        64);
//    case LiteralKind::This: {
//        auto* address = getAddressImpl(lit);
//        return loadAddress(address, mapType(lit.type()->base()), "__this");
//    }
//    case LiteralKind::String:
//        SC_DEBUGFAIL();
//    }
//}
//
// ir::Value* LoweringContext::getValueImpl(UnaryPrefixExpression const& expr) {
//    switch (expr.operation()) {
//    case ast::UnaryPrefixOperator::Increment:
//        [[fallthrough]];
//    case ast::UnaryPrefixOperator::Decrement: {
//        ir::Value* const operandAddress = getAddress(*expr.operand());
//        SC_ASSERT(isa<ir::PointerType>(operandAddress->type()),
//                  "We need a pointer to store to");
//        ir::Type const* arithmeticType =
//            mapType(expr.operand()->type()->base());
//        ir::Value* operandValue =
//            loadAddress(operandAddress,
//                        arithmeticType,
//                        utl::strcat(expr.operation(), ".value"));
//        auto const operation =
//            expr.operation() == ast::UnaryPrefixOperator::Increment ?
//                ir::ArithmeticOperation::Add :
//                ir::ArithmeticOperation::Sub;
//        auto* newValue =
//            new ir::ArithmeticInst(operandValue,
//                                   irCtx.integralConstant(APInt(1, 64)),
//                                   operation,
//                                   utl::strcat(expr.operation(), ".result"));
//        currentBB()->pushBack(newValue);
//        auto* store = new ir::Store(irCtx, operandAddress, newValue);
//        currentBB()->pushBack(store);
//        return newValue;
//    }
//    case ast::UnaryPrefixOperator::Promotion:
//        return getValue(*expr.operand());
//    case ast::UnaryPrefixOperator::Negation: {
//        ir::Value* const operand = getValue(*expr.operand());
//        SC_ASSERT(isa<ir::ArithmeticType>(operand->type()),
//                  "The other unary operators operate on arithmetic values");
//        auto* zero     = irCtx.arithmeticConstant(0, operand->type());
//        auto operation = isa<ir::IntegralType>(operand->type()) ?
//                             ir::ArithmeticOperation::Sub :
//                             ir::ArithmeticOperation::FSub;
//        auto* inst =
//            new ir::ArithmeticInst(zero, operand, operation, "negated");
//        currentBB()->pushBack(inst);
//        return inst;
//    }
//    default: {
//        ir::Value* const operand = getValue(*expr.operand());
//        SC_ASSERT(isa<ir::ArithmeticType>(operand->type()),
//                  "The other unary operators operate on arithmetic values");
//        auto* inst =
//            new ir::UnaryArithmeticInst(irCtx,
//                                        operand,
//                                        mapUnaryArithmeticOp(expr.operation()),
//                                        std::string("expr.result"));
//        currentBB()->pushBack(inst);
//        return inst;
//    }
//    }
//}
//
// ir::Value* LoweringContext::getValueImpl(BinaryExpression const& expr) {
//    auto* structType =
//        cast<sema::StructureType const*>(expr.lhs()->type()->base());
//
//    switch (expr.operation()) {
//        using enum BinaryOperator;
//    case Multiplication:
//        [[fallthrough]];
//    case Division:
//        [[fallthrough]];
//    case Remainder:
//        [[fallthrough]];
//    case Addition:
//        [[fallthrough]];
//    case Subtraction:
//        [[fallthrough]];
//    case LeftShift:
//        [[fallthrough]];
//    case RightShift:
//        [[fallthrough]];
//    case BitwiseAnd:
//        [[fallthrough]];
//    case BitwiseXOr:
//        [[fallthrough]];
//    case BitwiseOr: {
//        ir::Value* const lhs = getValue(*expr.lhs());
//        ir::Value* const rhs = getValue(*expr.rhs());
//        auto* type           = lhs->type();
//        if (expr.operation() != LeftShift && expr.operation() != RightShift) {
//            SC_ASSERT(lhs->type() == rhs->type(),
//                      "Need same types to do arithmetic");
//            SC_ASSERT(isa<ir::ArithmeticType>(type),
//                      "Need arithmetic type to do arithmetic");
//        }
//        else {
//            SC_ASSERT(isa<ir::IntegralType>(lhs->type()),
//                      "Need integral type for shift");
//            SC_ASSERT(isa<ir::IntegralType>(rhs->type()),
//                      "Need integral type for shift");
//        }
//        auto operation = mapArithmeticOp(structType, expr.operation());
//        auto* arithInst =
//            new ir::ArithmeticInst(lhs, rhs, operation, "expr.result");
//        currentBB()->pushBack(arithInst);
//        return arithInst;
//    }
//    case LogicalAnd:
//        [[fallthrough]];
//    case LogicalOr: {
//        ir::Value* const lhs = getValue(*expr.lhs());
//        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
//        auto* startBlock = currentBB();
//        auto* rhsBlock   = new ir::BasicBlock(irCtx, "log.rhs");
//        auto* endBlock   = new ir::BasicBlock(irCtx, "log.end");
//        currentBB()->pushBack(
//            expr.operation() == LogicalAnd ?
//                new ir::Branch(irCtx, lhs, rhsBlock, endBlock) :
//                new ir::Branch(irCtx, lhs, endBlock, rhsBlock));
//        currentFunction->pushBack(rhsBlock);
//        setCurrentBB(rhsBlock);
//        auto* rhs = getValue(*expr.rhs());
//        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
//        currentBB()->pushBack(new ir::Goto(irCtx, endBlock));
//        currentFunction->pushBack(endBlock);
//        setCurrentBB(endBlock);
//        auto* result =
//            expr.operation() == LogicalAnd ?
//                new ir::Phi({ { startBlock, irCtx.integralConstant(0, 1) },
//                              { rhsBlock, rhs } },
//                            "log.and.value") :
//                new ir::Phi({ { startBlock, irCtx.integralConstant(1, 1) },
//                              { rhsBlock, rhs } },
//                            "log.or.value");
//        currentBB()->pushBack(result);
//        return result;
//    }
//    case Less:
//        [[fallthrough]];
//    case LessEq:
//        [[fallthrough]];
//    case Greater:
//        [[fallthrough]];
//    case GreaterEq:
//        [[fallthrough]];
//    case Equals:
//        [[fallthrough]];
//    case NotEquals: {
//        auto* lhs = getValue(*expr.lhs());
//        auto* rhs = getValue(*expr.rhs());
//        SC_ASSERT(lhs->type() == rhs->type(), "Need same type for
//        comparison"); auto* cmpInst = new ir::CompareInst(irCtx,
//                                            lhs,
//                                            rhs,
//                                            mapCompareMode(structType),
//                                            mapCompareOp(expr.operation()),
//                                            "cmp.result");
//        currentBB()->pushBack(cmpInst);
//        return cmpInst;
//    }
//    case Comma: {
//        getValue(*expr.lhs());
//        return getValue(*expr.rhs());
//    }
//    case Assignment:
//        [[fallthrough]];
//    case AddAssignment:
//        [[fallthrough]];
//    case SubAssignment:
//        [[fallthrough]];
//    case MulAssignment:
//        [[fallthrough]];
//    case DivAssignment:
//        [[fallthrough]];
//    case RemAssignment:
//        [[fallthrough]];
//    case LSAssignment:
//        [[fallthrough]];
//    case RSAssignment:
//        [[fallthrough]];
//    case AndAssignment:
//        [[fallthrough]];
//    case OrAssignment:
//        [[fallthrough]];
//    case XOrAssignment: {
//        auto* rhs = getValue(*expr.rhs());
//        /// If user assign an implicit reference, sema should have inserted an
//        /// `ImplicitConversion` node that gives us a non-reference type here
//        if (expr.rhs()->type()->isReference()) {
//            /// Here we want to reassign the reference
//            SC_ASSERT(isa<ir::PointerType>(rhs->type()),
//                      "Explicit reference must resolve to pointer");
//            SC_ASSERT(expr.operation() == Assignment,
//                      "Can't do arithmetic with addresses");
//            auto* lhs   = getReferenceAddress(*expr.lhs());
//            auto* store = new ir::Store(irCtx, lhs, rhs);
//            currentBB()->pushBack(store);
//            return store;
//        }
//        SC_ASSERT(!isa<ir::PointerType>(rhs->type()),
//                  "Here rhs shall not be a pointer");
//        auto* lhs = getAddress(*expr.lhs());
//        SC_ASSERT(isa<ir::PointerType>(lhs->type()),
//                  "Need pointer to assign to variable");
//        auto* structType =
//            cast<sema::StructureType const*>(expr.lhs()->type()->base());
//        auto* assignee = [&]() -> ir::Value* {
//            if (expr.operation() == Assignment) {
//                return rhs;
//            }
//            auto* lhsValue = loadAddress(lhs, mapType(structType),
//            "lhs.value"); auto* arithmeticResult =
//                new ir::ArithmeticInst(lhsValue,
//                                       rhs,
//                                       mapArithmeticAssignOp(structType,
//                                                             expr.operation()),
//                                       "expr.result");
//            currentBB()->pushBack(arithmeticResult);
//            return arithmeticResult;
//        }();
//        auto* store = new ir::Store(irCtx, lhs, assignee);
//        currentBB()->pushBack(store);
//        return store;
//    }
//    case BinaryOperator::_count:
//        SC_UNREACHABLE();
//    }
//}
//
// ir::Value* LoweringContext::getValueImpl(MemberAccess const& expr) {
//    return loadAddress(getAddressImpl(expr),
//                       mapType(expr.type()),
//                       "member.access");
//}
//
// ir::Value* LoweringContext::getValueImpl(ReferenceExpression const& expr) {
//    SC_ASSERT(expr.referred()->type()->isReference() ||
//                  expr.referred()->isLValue(),
//              "");
//    return getAddress(*expr.referred());
//}
//
// ir::Value* LoweringContext::getValueImpl(UniqueExpression const& expr) {
//    SC_DEBUGFAIL();
//}
//
// ir::Value* LoweringContext::getValueImpl(Conditional const& condExpr) {
//    auto* cond      = getValue(*condExpr.condition());
//    auto* thenBlock = new ir::BasicBlock(irCtx, "conditional.then");
//    auto* elseBlock = new ir::BasicBlock(irCtx, "conditional.else");
//    auto* endBlock  = new ir::BasicBlock(irCtx, "conditional.end");
//    currentBB()->pushBack(new ir::Branch(irCtx, cond, thenBlock, elseBlock));
//    currentFunction->pushBack(thenBlock);
//    /// Generate then block.
//    setCurrentBB(thenBlock);
//    auto* thenVal = getValue(*condExpr.thenExpr());
//    thenBlock     = currentBB();
//    thenBlock->pushBack(new ir::Goto(irCtx, endBlock));
//    currentFunction->pushBack(elseBlock);
//    /// Generate else block.
//    setCurrentBB(elseBlock);
//    auto* elseVal = getValue(*condExpr.elseExpr());
//    elseBlock     = currentBB();
//    elseBlock->pushBack(new ir::Goto(irCtx, endBlock));
//    currentFunction->pushBack(endBlock);
//    /// Generate end block.
//    setCurrentBB(endBlock);
//    auto* result =
//        new ir::Phi({ { thenBlock, thenVal }, { elseBlock, elseVal } },
//                    "conditional.result");
//    currentBB()->pushBack(result);
//    return result;
//}
//
// ir::Value* LoweringContext::getValueImpl(FunctionCall const& expr) {
//    auto* call = genCallImpl(expr);
//    if (expr.type()->isReference()) {
//        return loadAddress(call, mapType(expr.type()->base()), "call.value");
//    }
//    return call;
//}
//
// ir::Value* LoweringContext::genCallImpl(FunctionCall const& expr) {
//    ir::Callable* function = functionMap.find(expr.function())->second;
//    auto args              = mapArguments(expr.arguments());
//    if (expr.isMemberCall) {
//        auto* object = cast<MemberAccess const*>(expr.object())->object();
//        args.insert(args.begin(), getAddress(*object));
//    }
//    auto* call =
//        new ir::Call(function,
//                     args,
//                     expr.type()->base() != symTable.Void() ? "call.result" :
//                                                              std::string{});
//    currentBB()->pushBack(call);
//    return call;
//}
//
// ir::Value* LoweringContext::getValueImpl(Subscript const& expr) {
//    auto* address = getAddress(expr);
//    return loadAddress(address, mapType(expr.type()->base()), "[].value");
//}
//
// ir::Value* LoweringContext::getValueImpl(ImplicitConversion const& conv) {
//    return getValue(*conv.expression());
//    auto* expr = conv.expression();
//    if (conv.type()->isReference()) {
//        return getAddress(*expr);
//    }
//    return getValue(*expr);
//}
//
// ir::Value* LoweringContext::getValueImpl(ListExpression const& list) {
//    SC_DEBUGFAIL();
//}
