#include "IRGen/FunctionGeneration.h"

#include <utl/function_view.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/Maps.h"
#include "IRGen/Utility.h"
#include "IRGen/Value.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"
#include "Sema/QualType.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;
using enum ValueLocation;
using enum ValueRepresentation;

static std::string nameFromSourceLoc(std::string_view name,
                                     SourceLocation loc) {
    return utl::strcat(name, ".at.", loc.line, ".", loc.column);
}

namespace {

struct LoopDesc {
    ir::BasicBlock* header = nullptr;
    ir::BasicBlock* body = nullptr;
    ir::BasicBlock* inc = nullptr;
    ir::BasicBlock* end = nullptr;
};

struct FuncGenContext: FuncGenContextBase {
    utl::stack<LoopDesc, 4> loopStack;

    FuncGenContext(auto&... args): FuncGenContextBase(args...) {}

    /// # Statements
    SC_NODEBUG void generate(ast::Statement const&);

    void generateImpl(ast::Statement const&) { SC_UNREACHABLE(); }
    // void generateImpl(ast::ImportStatement const&);
    void generateImpl(ast::CompoundStatement const&);
    void generateImpl(ast::FunctionDefinition const&);
    void generateParameter(ast::ParameterDeclaration const* paramDecl,
                           PassingConvention pc,
                           List<ir::Parameter>::iterator& paramItr);
    void generateImpl(ast::VariableDeclaration const&);
    void generateImpl(ast::ExpressionStatement const&);
    // void generateImpl(ast::EmptyStatement const&) {}
    void generateImpl(ast::ReturnStatement const&);
    // void generateImpl(ast::IfStatement const&);
    // void generateImpl(ast::LoopStatement const&);
    // void generateImpl(ast::JumpStatement const&);

    /// # Statement specific utilities
    void callDtor(sema::Object const* object,
                  sema::Function const* dtor,
                  ast::ASTNode const& sourceNode);
    void emitDtorCalls(sema::DtorStack const& dtorStack,
                       ast::ASTNode const& sourceNode);

    /// Creates array size values and stores them in `objectMap` if declared
    /// type is array
    void generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                  sema::Object const* initObject);

    /// # Expressions
    SC_NODEBUG Value getValue(ast::Expression const* expr);

    /// \Returns single value if `Repr == Packed` or vector of values if `Repr
    /// == Unpacked`
    template <ValueRepresentation Repr>
    SC_NODEBUG auto getValue(ValueLocation loc, ast::Expression const* expr) {
        return to<Repr>(loc, getValue(expr));
    }

    Value getValueImpl(ast::Expression const&) { SC_UNREACHABLE(); }
    Value getValueImpl(ast::Identifier const&);
    Value getValueImpl(ast::Literal const&);
    // Value getValueImpl(ast::UnaryExpression const&);
    Value getValueImpl(ast::BinaryExpression const&);
    Value getValueImpl(ast::MemberAccess const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Variable const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Property const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Temporary const&) {
        SC_UNREACHABLE();
    }
    Value getValueImpl(ast::DereferenceExpression const&);
    Value getValueImpl(ast::AddressOfExpression const&);
    Value getValueImpl(ast::Conditional const&);
    Value getValueImpl(ast::FunctionCall const&);
    Value getValueImpl(ast::Subscript const&);
    // Value getValueImpl(ast::SubscriptSlice const&);
    Value getValueImpl(ast::ListExpression const&);
    bool genStaticListData(ast::ListExpression const& list, ir::Alloca* dest);
    void genDynamicListData(ast::ListExpression const& list, ir::Alloca* dest);
    // Value getValueImpl(ast::MoveExpr const&);
    // Value getValueImpl(ast::UniqueExpr const&);
    Value getValueImpl(ast::Conversion const&);
    // Value getValueImpl(ast::UninitTemporary const&);
    Value getValueImpl(ast::ConstructExpr const&);
    // Value genConstructDynArray(ast::ConstructExpr const&,
    //                            sema::ArrayType const&);
    // Value genConstructNonTrivial(ast::ConstructExpr const&);
    SC_NODEBUG Value genConstructTrivial(ast::ConstructExpr const&,
                                         sema::Type const&);
    Value genConstructTrivialImpl(ast::ConstructExpr const&,
                                  sema::ObjectType const&);
    Value genConstructTrivialImpl(ast::ConstructExpr const&,
                                  sema::StructType const&);
    Value genConstructTrivialImpl(ast::ConstructExpr const&,
                                  sema::ArrayType const&);
    Value genConstructTrivialImpl(ast::ConstructExpr const&,
                                  sema::Type const&) {
        SC_UNREACHABLE();
    }
    // Value getValueImpl(ast::NonTrivAssignExpr const&);

    /// # General utilities

    /// Add source location of \p expr to \p inst
    void addSourceLoc(ir::Instruction* inst, ast::ASTNode const& sourceNode);
};

} // namespace

void irgen::generateAstFunction(Config config, FuncGenParameters params) {
    FuncGenContext funcCtx(config, params);
    funcCtx.generate(*params.semaFn.definition());
}

/// MARK: - Statements

void FuncGenContext::generate(ast::Statement const& node) {
    visit(node,
          [this](auto const& node) SC_NODEBUG { return generateImpl(node); });
}

// void FuncGenContext::generateImpl(ast::ImportStatement const&) {
//     /// No-op
// }

void FuncGenContext::generateImpl(ast::CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    emitDtorCalls(cmpStmt.dtorStack(), cmpStmt);
}

static sema::SpecialLifetimeFunction toSLFKindToGenerate(
    sema::SpecialMemberFunction kind) {
    using enum sema::SpecialMemberFunction;
    using enum sema::SpecialLifetimeFunction;
    if (kind == Delete) {
        return Destructor;
    }
    return DefaultConstructor;
}

void FuncGenContext::generateImpl(ast::FunctionDefinition const& def) {
    /// If the function is a user defined special member function (constructor
    /// or destructor) we still generate the code of the non-user defined
    /// equivalent function and then append the user defined code. This way in a
    /// user defined destructor all member destructors get called and in a user
    /// defined constructor all member variables get initialized automatically
    if (auto md = semaFn.getSMFMetadata()) {
        auto kind = toSLFKindToGenerate(md->kind());
        generateSynthFunctionAs(kind, config, *this);
        makeBlockCurrent(&irFn.back());
    }
    else {
        addNewBlock("entry");
    }
    /// Here we add all parameters to our value map and store possibly mutable
    /// parameters (everything that's not a reference) to the stack
    auto CC = getCC(&semaFn);
    auto irParamItr = irFn.parameters().begin();
    if (CC.returnValue().location() == Memory) {
        ++irParamItr;
    }
    for (auto [paramDecl, pc]:
         ranges::views::zip(def.parameters(), CC.arguments()))
    {
        generateParameter(paramDecl, pc, irParamItr);
    }
    generate(*def.body());
    insertAllocas();
}

static sema::ObjectType const* stripRef(sema::Type const* type) {
    if (auto* ref = dyncast<sema::ReferenceType const*>(type)) {
        return ref->base().get();
    }
    return cast<sema::ObjectType const*>(type);
}

void FuncGenContext::generateParameter(
    ast::ParameterDeclaration const* paramDecl,
    PassingConvention pc,
    List<ir::Parameter>::iterator& irParamItr) {
    auto params = utl::small_vector<ir::Value*>(pc.numParams(), [&]() {
        return std::to_address(irParamItr++);
    });
    std::string name = isa<ast::ThisParameter>(paramDecl) ?
                           "this" :
                           std::string(paramDecl->name());
    auto* semaParam = paramDecl->object();
    auto* paramType = stripRef(semaParam->type());
    switch (pc.location()) {
    case Register: {
        /// Reference parameters are special: We don't store them to memory
        /// because they cannot be reassigned
        if (isa<sema::ReferenceType>(semaParam->type())) {
            valueMap.insert(semaParam,
                            Value(name,
                                  paramType,
                                  params,
                                  Memory,
                                  params.size() == 1 ? Packed : Unpacked));
        }
        else if (params.size() == 1) {
            auto* val = params.front();
            auto* mem = storeToMemory(val, name);
            valueMap.insert(semaParam,
                            Value(name,
                                  paramType,
                                  ValueArray{ mem },
                                  Memory,
                                  Unpacked));
        }
        else {
            auto* packedVal = packValues(params, name);
            auto* mem = storeToMemory(packedVal, name);
            valueMap.insert(semaParam,
                            Value(name, paramType, { mem }, Memory, Packed));
        }
        break;
    }

    case Memory:
        SC_ASSERT(params.size() == 1, "");
        valueMap.insert(semaParam,
                        Value(name, paramType, params, Memory, Unpacked));
        break;
    }
}

// void FuncGenContext::generateVarDeclArraySize(ast::VarDeclBase const*
// varDecl,
//                                               sema::Object const* initObject)
//                                               {
//     if (!isFatPointer(varDecl->type())) {
//         return;
//     }
//     auto size = valueMap.arraySize(initObject);
//     if (isa<sema::ReferenceType>(varDecl->type())) {
//         valueMap.insertArraySize(varDecl->variable(),
//                                  Value(toRegister(size, *varDecl),
//                                  Register));
//     }
//     else {
//         auto* newSize = storeToMemory(toRegister(size, *varDecl),
//                                       utl::strcat(varDecl->name(), ".size"));
//         valueMap.insertArraySize(varDecl->variable(),
//                                  Value(newSize, size.type(), Memory));
//     }
// }

void FuncGenContext::generateImpl(ast::VariableDeclaration const& varDecl) {
    auto dtorStack = varDecl.dtorStack();
    auto* var = varDecl.variable();
    std::string name = std::string(var->name());
    if (isa<sema::ReferenceType>(var->type())) {
        auto value = getValue(varDecl.initExpr());
        valueMap.insert(var, value);
    }
    else {
        auto* address = getValue<Packed>(Memory, varDecl.initExpr());
        address->setName(utl::strcat(name, ".addr"));
        valueMap.insert(var,
                        Value(name,
                              varDecl.initExpr()->type().get(),
                              { address },
                              Memory,
                              Packed));
    }
    emitDtorCalls(dtorStack, varDecl);
}

void FuncGenContext::generateImpl(
    ast::ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    emitDtorCalls(exprStatement.dtorStack(), exprStatement);
}

void FuncGenContext::generateImpl(ast::ReturnStatement const& retStmt) {
    /// Simple case of `return;` in a void function
    if (!retStmt.expression()) {
        emitDtorCalls(retStmt.dtorStack(), retStmt);
        add<ir::Return>(ctx.voidValue());
        return;
    }

    /// More complex `return <value>;` case
    auto retval = getValue(retStmt.expression());
    auto targetLocation = getCC(&semaFn).returnValue().location();
    switch (targetLocation) {
    case Register: {
        /// Pointers we keep in registers but references directly refer to the
        /// value in memory
        auto destLocation =
            isa<sema::ReferenceType>(*semaFn.returnType()) ? Memory : Register;
        auto* value = to<Packed>(destLocation, retval);
        emitDtorCalls(retStmt.dtorStack(), retStmt);
        add<ir::Return>(value);
        return;
    }
    case Memory: {
        auto* retvalDest = &irFn.parameters().front();
        if (retval.isMemory()) {
            auto* retvalAddr = toUnpackedMemory(retval).front();
            if (auto* allocaInst = dyncast<ir::Alloca*>(retvalAddr)) {
                allocaInst->replaceAllUsesWith(retvalDest);
            }
            else {
                SC_ASSERT(retStmt.expression()->type()->hasTrivialLifetime(),
                          "We can only memcpy trivial lifetime types");
                callMemcpy(retvalDest, retvalAddr, retval.type()->size());
            }
        }
        else {
            add<ir::Store>(retvalDest, toPackedRegister(retval));
        }
        emitDtorCalls(retStmt.dtorStack(), retStmt);
        add<ir::Return>(ctx.voidValue());
        return;
    }
    }
}

// void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
//     auto* condition = getValue<Register>(stmt.condition());
//     emitDtorCalls(stmt.dtorStack(), stmt);
//     auto* thenBlock = newBlock("if.then");
//     auto* elseBlock = stmt.elseBlock() ? newBlock("if.else") : nullptr;
//     auto* endBlock = newBlock("if.end");
//     add<ir::Branch>(condition, thenBlock, elseBlock ? elseBlock : endBlock);
//     add(thenBlock);
//     generate(*stmt.thenBlock());
//     add<ir::Goto>(endBlock);
//     if (stmt.elseBlock()) {
//         add(elseBlock);
//         generate(*stmt.elseBlock());
//         add<ir::Goto>(endBlock);
//     }
//     add(endBlock);
// }

// void FuncGenContext::generateImpl(ast::LoopStatement const& loopStmt) {
//     switch (loopStmt.kind()) {
//     case ast::LoopKind::For: {
//         auto* loopHeader = newBlock("loop.header");
//         auto* loopBody = newBlock("loop.body");
//         auto* loopInc = newBlock("loop.inc");
//         auto* loopEnd = newBlock("loop.end");
//         generate(*loopStmt.varDecl());
//         add<ir::Goto>(loopHeader);
//
//         /// Header
//         add(loopHeader);
//         auto* condition = getValue<Register>(loopStmt.condition());
//         emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//         add<ir::Branch>(condition, loopBody, loopEnd);
//         loopStack.push({ .header = loopHeader,
//                          .body = loopBody,
//                          .inc = loopInc,
//                          .end = loopEnd });
//
//         /// Body
//         add(loopBody);
//         generate(*loopStmt.block());
//         add<ir::Goto>(loopInc);
//
//         /// Inc
//         add(loopInc);
//         getValue(loopStmt.increment());
//         emitDtorCalls(loopStmt.incrementDtorStack(), loopStmt);
//         add<ir::Goto>(loopHeader);
//
//         /// End
//         add(loopEnd);
//         loopStack.pop();
//         break;
//     }
//
//     case ast::LoopKind::While: {
//         auto* loopHeader = newBlock("loop.header");
//         auto* loopBody = newBlock("loop.body");
//         auto* loopEnd = newBlock("loop.end");
//         add<ir::Goto>(loopHeader);
//
//         /// Header
//         add(loopHeader);
//         auto* condition = getValue<Register>(loopStmt.condition());
//         emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//         add<ir::Branch>(condition, loopBody, loopEnd);
//         loopStack.push(
//             { .header = loopHeader, .body = loopBody, .end = loopEnd });
//
//         /// Body
//         add(loopBody);
//         generate(*loopStmt.block());
//         add<ir::Goto>(loopHeader);
//
//         /// End
//         add(loopEnd);
//         loopStack.pop();
//         break;
//     }
//
//     case ast::LoopKind::DoWhile: {
//         auto* loopBody = newBlock("loop.body");
//         auto* loopFooter = newBlock("loop.footer");
//         auto* loopEnd = newBlock("loop.end");
//         add<ir::Goto>(loopBody);
//         loopStack.push(
//             { .header = loopFooter, .body = loopBody, .end = loopEnd });
//
//         /// Body
//         add(loopBody);
//         generate(*loopStmt.block());
//         add<ir::Goto>(loopFooter);
//
//         /// Footer
//         add(loopFooter);
//         auto* condition = getValue<Register>(loopStmt.condition());
//         emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//         add<ir::Branch>(condition, loopBody, loopEnd);
//
//         /// End
//         add(loopEnd);
//         loopStack.pop();
//         break;
//     }
//     }
//     emitDtorCalls(loopStmt.dtorStack(), loopStmt);
// }

// void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
//     emitDtorCalls(jump.dtorStack(), jump);
//     auto* dest = [&] {
//         auto& currentLoop = loopStack.top();
//         switch (jump.kind()) {
//         case ast::JumpStatement::Break:
//             return currentLoop.end;
//         case ast::JumpStatement::Continue:
//             return currentLoop.inc ? currentLoop.inc : currentLoop.header;
//         }
//         SC_UNREACHABLE();
//     }();
//     add<ir::Goto>(dest);
// }

/// MARK: - Expressions

/// Only used for assertions
[[maybe_unused]] static bool isIntType(size_t width, ir::Type const* type) {
    return cast<ir::IntegralType const*>(type)->bitwidth() == width;
}

Value FuncGenContext::getValue(ast::Expression const* expr) {
    SC_EXPECT(expr);
    auto result =
        visit(*expr, [&](auto& expr) SC_NODEBUG { return getValueImpl(expr); });
    valueMap.tryInsert(expr->object(), result);
    return result;
}

Value FuncGenContext::getValueImpl(ast::Identifier const& id) {
    return valueMap(id.object());
}

Value FuncGenContext::getValueImpl(ast::Literal const& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        return Value("int.lit",
                     lit.type().get(),
                     { ctx.intConstant(lit.value<APInt>()) },
                     Register,
                     Packed);
    case Boolean:
        return Value("bool.lit",
                     lit.type().get(),
                     { ctx.intConstant(lit.value<APInt>()) },
                     Register,
                     Packed);
    case FloatingPoint:
        return Value("float.lit",
                     lit.type().get(),
                     { ctx.floatConstant(lit.value<APFloat>()) },
                     Register,
                     Packed);
    case Null:
        return Value("null.lit",
                     lit.type().get(),
                     { ctx.nullpointer() },
                     Register,
                     Packed);
    case This:
        return valueMap(lit.object());

    case String: {
        auto const& text = lit.value<std::string>();
        auto name = nameFromSourceLoc("string", lit.sourceLocation());
        auto* data = ctx.stringLiteral(text);
        auto* global = mod.makeGlobalConstant(ctx, data, name);
        SC_UNIMPLEMENTED(); // Is this static or dynamic array?
        //        return Value(name, lit.type().get(), global, Memory, );
    }
    case Char:
        return Value("char.lit",
                     lit.type().get(),
                     { ctx.intConstant(lit.value<APInt>()) },
                     Register,
                     Packed);
    }
    SC_UNREACHABLE();
}

// Value FuncGenContext::getValueImpl(ast::UnaryExpression const& expr) {
//     using enum ast::UnaryOperator;
//     switch (expr.operation()) {
//     case Increment:
//         [[fallthrough]];
//     case Decrement: {
//         Value operand = getValue(expr.operand());
//         SC_ASSERT(operand.isMemory(),
//                   "Operand must be in memory to be modified");
//         ir::Value* opAddr = toMemory(operand, expr);
//         ir::Type const* operandType = typeMap(expr.operand()->type());
//         ir::Value* operandValue = toRegister(operand, expr);
//         auto* newValue =
//             add<ir::ArithmeticInst>(operandValue,
//                                     ctx.arithmeticConstant(1, operandType),
//                                     expr.operation() == Increment ?
//                                         ir::ArithmeticOperation::Add :
//                                         ir::ArithmeticOperation::Sub,
//                                     utl::strcat(expr.operation(), ".res"));
//         add<ir::Store>(opAddr, newValue);
//         switch (expr.notation()) {
//         case ast::UnaryOperatorNotation::Prefix:
//             return operand;
//         case ast::UnaryOperatorNotation::Postfix:
//             return Value(operandValue, Register);
//         }
//     }
//
//     case ast::UnaryOperator::Promotion:
//         return getValue(expr.operand());
//
//     case ast::UnaryOperator::Negation: {
//         auto* operand = getValue<Register>(expr.operand());
//         auto operation = isa<sema::IntType>(expr.operand()->type().get()) ?
//                              ir::ArithmeticOperation::Sub :
//                              ir::ArithmeticOperation::FSub;
//         auto* newValue =
//             add<ir::ArithmeticInst>(ctx.arithmeticConstant(0,
//             operand->type()),
//                                     operand,
//                                     operation,
//                                     "negated");
//         return Value(newValue, Register);
//     }
//
//     default:
//         auto* operand = getValue<Register>(expr.operand());
//         auto* newValue =
//             add<ir::UnaryArithmeticInst>(operand,
//                                          mapUnaryOp(expr.operation()),
//                                          "expr");
//         return Value(newValue, Register);
//     }
// }

Value FuncGenContext::getValueImpl(ast::BinaryExpression const& expr) {
    auto* type = expr.lhs()->type().get();
    auto resName = binaryOpResultName(expr.operation());
    using enum ast::BinaryOperator;
    switch (expr.operation()) {
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
        SC_UNIMPLEMENTED();
        //        auto* lhs = getValue<Register>(expr.lhs());
        //        auto* rhs = getValue<Register>(expr.rhs());
        //        auto operation = mapArithmeticOp(type, expr.operation());
        //        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation,
        //        resName); return Value(result, Register);
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        SC_UNIMPLEMENTED();
        //        ir::Value* const lhs = getValue<Register>(expr.lhs());
        //        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical
        //        operation"); auto* startBlock = &currentBlock(); auto*
        //        rhsBlock = newBlock("log.rhs"); auto* endBlock =
        //        newBlock("log.end"); if (expr.operation() == LogicalAnd) {
        //            add<ir::Branch>(lhs, rhsBlock, endBlock);
        //        }
        //        else {
        //            add<ir::Branch>(lhs, endBlock, rhsBlock);
        //        }
        //
        //        add(rhsBlock);
        //        auto* rhs = getValue<Register>(expr.rhs());
        //        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical
        //        operation"); add<ir::Goto>(endBlock); add(endBlock);
        //
        //        if (expr.operation() == LogicalAnd) {
        //            auto* result = add<ir::Phi>(
        //                std::array<ir::PhiMapping, 2>{
        //                    ir::PhiMapping{ startBlock, ctx.boolConstant(0) },
        //                    ir::PhiMapping{ rhsBlock, rhs } },
        //                "log.and");
        //            return Value(result, Register);
        //        }
        //        else {
        //            auto* result = add<ir::Phi>(
        //                std::array<ir::PhiMapping, 2>{
        //                    ir::PhiMapping{ startBlock, ctx.boolConstant(1) },
        //                    ir::PhiMapping{ rhsBlock, rhs } },
        //                "log.or");
        //            return Value(result, Register);
        //        }
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
        SC_UNIMPLEMENTED();
        //        auto* result =
        //            add<ir::CompareInst>(toThinPointer(getValue<Packed>(expr.lhs())),
        //                                 toThinPointer(getValue<Packed>(expr.rhs())),
        //                                 mapCompareMode(type),
        //                                 mapCompareOp(expr.operation()),
        //                                 resName);
        //        return Value(result, Register);
    }

    case Comma:
        (void)getValue(expr.lhs());  /// Evaluate and discard LHS
        return getValue(expr.rhs()); /// Evaluate and return RHS

    case Assignment: {
        auto* lhs = getValue<Packed>(Memory, expr.lhs());
        auto* rhs = getValue<Packed>(Register, expr.rhs());
        add<ir::Store>(lhs, rhs);
        return Value("assignment.result",
                     symbolTable.Void(),
                     { ctx.voidValue() },
                     Register,
                     Packed);
    }
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
        SC_UNIMPLEMENTED();
        //        auto lhs = getValue(expr.lhs());
        //        SC_ASSERT(lhs.isMemory(), "");
        //        auto rhs = getValue<Register>(expr.rhs());
        //        SC_ASSERT(type == expr.rhs()->type().get(), "");
        //        auto operation = mapArithmeticAssignOp(type,
        //        expr.operation()); rhs =
        //        add<ir::ArithmeticInst>(toRegister(lhs, expr),
        //                                      rhs,
        //                                      operation,
        //                                      resName);
        //        add<ir::Store>(lhs.get(), rhs);
        //        return Value();
    }
    }
    SC_UNREACHABLE();
}

Value FuncGenContext::getValueImpl(ast::MemberAccess const& expr) {
    return visit(*expr.member()->object(),
                 [&](auto& obj) { return genMemberAccess(expr, obj); });
}

Value FuncGenContext::genMemberAccess(ast::MemberAccess const& expr,
                                      sema::Variable const& var) {
    Value base = getValue(expr.accessed());
    auto& metaData =
        typeMap.metaData(expr.accessed()->type().get()).members[var.index()];
    std::string name = "mem.acc";
    auto baseLoc = base.location();
    auto* baseVal = to<Unpacked>(baseLoc, base).front();
    auto values = zip(iota(metaData.beginIndex), metaData.fieldTypes) |
                  transform([&](auto p) -> ir::Value* {
        auto [index, type] = p;
        switch (baseLoc) {
        case Register:
            return add<ir::ExtractValue>(baseVal, IndexArray{ index }, name);
        case Memory:
            ir::Value* value =
                add<ir::GetElementPointer>(typeMap.packed(base.type()),
                                           baseVal,
                                           ctx.intConstant(0, 64),
                                           IndexArray{ index },
                                           name);
            if (index > metaData.beginIndex) {
                value = add<ir::Load>(value, type, name);
            }
            return value;
        }
    }) | ToSmallVector<2>;
    return Value(name, expr.type().get(), values, baseLoc, Unpacked);
}

Value FuncGenContext::genMemberAccess(ast::MemberAccess const& expr,
                                      sema::Property const& prop) {
    using enum sema::PropertyKind;
    switch (prop.kind()) {
    case ArraySize: {
        return getArraySize(expr.accessed()->type().get(),
                            getValue(expr.accessed()));
    }
    case ArrayEmpty: {
        SC_UNIMPLEMENTED();
        //        auto* arrayType =
        //            cast<sema::ArrayType
        //            const*>(expr.accessed()->type().get());
        //        if (arrayType->isDynamic()) {
        //            getValue(expr.accessed());
        //            auto size = valueMap.arraySize(expr.accessed()->object());
        //            auto* empty = add<ir::CompareInst>(toRegister(size, expr),
        //                                               ctx.intConstant(0, 64),
        //                                               ir::CompareMode::Signed,
        //                                               ir::CompareOperation::Equal,
        //                                               "empty");
        //            return Value(empty, Register);
        //        }
        //        else {
        //            return Value(ctx.boolConstant(arrayType->count() == 0),
        //            Register);
        //        }
    }
    case ArrayFront:
        [[fallthrough]];
    case ArrayBack: {
        SC_UNIMPLEMENTED();
        //        // TODO: Check that array is not empty
        //        auto* arrayType =
        //            cast<sema::ArrayType
        //            const*>(expr.accessed()->type().get());
        //        auto array = getValue(expr.accessed());
        //        switch (array.location()) {
        //        case Register: {
        //            SC_ASSERT(!arrayType->isDynamic(),
        //                      "Can't have dynamic array in register");
        //            size_t index = prop.kind() == ArrayFront ? 0 :
        //                                                       arrayType->count()
        //                                                       - 1;
        //
        //            if (isFatPointer(arrayType->elementType())) {
        //                auto* data = add<ir::ExtractValue>(array.get(),
        //                                                   IndexArray{ index,
        //                                                   0 },
        //                                                   "array.front.data");
        //                auto* size = add<ir::ExtractValue>(array.get(),
        //                                                   IndexArray{ index,
        //                                                   1 },
        //                                                   "array.front.size");
        //                valueMap.insertArraySize(expr.object(), Value(size,
        //                Register)); return Value(data, Register);
        //            }
        //            else {
        //                auto* elem = add<ir::ExtractValue>(array.get(),
        //                                                   IndexArray{ index
        //                                                   }, "array.front");
        //                return Value(elem, Register);
        //            }
        //        }
        //        case Memory: {
        //            auto* irElemType = typeMap(arrayType->elementType());
        //            auto* index = [&]() -> ir::Value* {
        //                if (prop.kind() == ArrayFront) {
        //                    return ctx.intConstant(0, 64);
        //                }
        //                if (!arrayType->isDynamic()) {
        //                    return ctx.intConstant(arrayType->count() - 1,
        //                    64);
        //                }
        //                auto count =
        //                valueMap.arraySize(expr.accessed()->object()); return
        //                add<ir::ArithmeticInst>(toRegister(count, expr),
        //                                               ctx.intConstant(1, 64),
        //                                               ir::ArithmeticOperation::Sub,
        //                                               "back.index");
        //            }();
        //            if (isFatPointer(arrayType->elementType())) {
        //                auto* arrayView = arrayPtrType;
        //                auto* irSizeType = ctx.intType(64);
        //                auto* data = add<ir::GetElementPointer>(arrayView,
        //                                                        array.get(),
        //                                                        index,
        //                                                        IndexArray{ 0
        //                                                        },
        //                                                        "array.front.data");
        //                auto* size = add<ir::GetElementPointer>(arrayView,
        //                                                        array.get(),
        //                                                        index,
        //                                                        IndexArray{ 1
        //                                                        },
        //                                                        "array.front.size");
        //                valueMap.insertArraySize(expr.object(),
        //                                         Value(size, irSizeType,
        //                                         Memory));
        //                return Value(data, irElemType, Memory);
        //            }
        //            else {
        //                auto* elem = add<ir::GetElementPointer>(irElemType,
        //                                                        array.get(),
        //                                                        index,
        //                                                        IndexArray{},
        //                                                        "array.front");
        //                return Value(elem, irElemType, Memory);
        //            }
        //        }
        //        }
    }
    default:
        SC_UNREACHABLE();
    }
}

Value FuncGenContext::getValueImpl(ast::DereferenceExpression const& expr) {
    SC_EXPECT(isa<sema::PointerType>(*expr.referred()->type()));
    auto value = getValue(expr.referred());
    return Value(utl::strcat(value.name(), ".deref"),
                 expr.type().get(),
                 toUnpackedRegister(value),
                 Memory,
                 Unpacked);
}

Value FuncGenContext::getValueImpl(ast::AddressOfExpression const& expr) {
    auto value = getValue(expr.referred());
    return Value(utl::strcat(value.name(), ".addr"),
                 expr.type().get(),
                 toUnpackedMemory(value),
                 Register,
                 Unpacked);
}

Value FuncGenContext::getValueImpl(ast::Conditional const& condExpr) {
    auto* cond = getValue<Packed>(Register, condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto thenVal = getValue(condExpr.thenExpr());
    thenBlock = &currentBlock(); /// Nested `?:` operands etc. may have changed
                                 /// `currentBlock`

    /// Generate else block.
    add(elseBlock);
    auto elseVal = getValue(condExpr.elseExpr());
    elseBlock = &currentBlock();

    /// Make common representation
    auto loc = commonLocation(thenVal.location(), elseVal.location(), Register);
    auto repr = commonRepresentation(thenVal.representation(),
                                     elseVal.representation(),
                                     Unpacked);
    auto thenResolved = withBlockCurrent(thenBlock, [&] {
        auto vals = to(loc, repr, thenVal);
        add<ir::Goto>(endBlock);
        return vals;
    });
    auto elseResolved = withBlockCurrent(elseBlock, [&] {
        auto vals = to(loc, repr, elseVal);
        add<ir::Goto>(endBlock);
        return vals;
    });
    SC_ASSERT(thenResolved.size() == elseResolved.size(),
              "Must be same representation");

    /// Generate end block.
    add(endBlock);
    auto phis = zip(thenResolved, elseResolved) |
                transform([&](auto p) -> ir::Value* {
        auto& [thenVal, elseVal] = p;
        return add<ir::Phi>(std::array{ ir::PhiMapping{ thenBlock, thenVal },
                                        ir::PhiMapping{ elseBlock, elseVal } },
                            "cond");
    }) | ToSmallVector<2>;
    return Value("cond", condExpr.type().get(), phis, loc, repr);
}

Value FuncGenContext::getValueImpl(ast::FunctionCall const& call) {
    ir::Callable* function = getFunction(call.function());
    auto name = [&]() -> std::string {
        if (isa<ir::VoidType>(function->returnType())) {
            return {};
        }
        return "call.result";
    }();
    auto retvalLocation = getCC(call.function()).returnValue().location();
    utl::small_vector<ir::Value*> irArguments;
    /// Allocate return value storage
    if (retvalLocation == Memory) {
        auto* irReturnType = typeMap.packed(call.function()->returnType());
        irArguments.push_back(
            makeLocalVariable(irReturnType, utl::strcat(name, ".addr")));
    }
    /// Unpack arguments
    for (auto [targetType, arg]:
         ranges::views::zip(call.function()->argumentTypes(), call.arguments()))
    {
        auto targetLoc = isa<sema::ReferenceType>(targetType) ? Memory :
                                                                Register;
        auto unpacked = getValue<Unpacked>(targetLoc, arg);
        irArguments.insert(irArguments.end(), unpacked.begin(), unpacked.end());
    }
    auto* callInst = add<ir::Call>(function, irArguments, name);
    auto* retval = retvalLocation == Memory ? irArguments.front() : callInst;
    return Value(name, call.type().get(), { retval }, retvalLocation, Packed);
}

Value FuncGenContext::getValueImpl(ast::Subscript const& expr) {
    auto* arrayAddr = getValue<Unpacked>(Memory, expr.callee()).front();
    auto* index = getValue<Packed>(Register, expr.argument(0));
    auto* elemType = typeMap.packed(expr.type().get());
    auto* elemAddr = add<ir::GetElementPointer>(elemType,
                                                arrayAddr,
                                                index,
                                                IndexArray{},
                                                "elem.addr");
    return Value("elem",
                 expr.type().get(),
                 ValueArray{ elemAddr },
                 Memory,
                 Packed);
}

// Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
//     auto* arrayType = cast<sema::ArrayType
//     const*>(expr.callee()->type().get()); auto* elemType =
//     typeMap(arrayType->elementType()); auto array = getValue(expr.callee());
//     auto lower = getValue<Register>(expr.lower());
//     auto upper = getValue<Register>(expr.upper());
//     SC_ASSERT(array.location() == Memory, "Must be in memory to be sliced");
//     auto* addr = add<ir::GetElementPointer>(elemType,
//                                             array.get(),
//                                             lower,
//                                             IndexArray{},
//                                             "elem.ptr");
//     Value result(addr, typeMap(expr.type()), Memory);
//     auto* size = add<ir::ArithmeticInst>(upper,
//                                          lower,
//                                          ir::ArithmeticOperation::Sub,
//                                          "slice.count");
//     valueMap.insertArraySize(expr.object(), Value(size, Register));
//     return result;
// }

static ir::Constant* evalConstant(ir::Context& ctx,
                                  ast::Expression const* expr) {
    if (auto* val = dyncast<sema::IntValue const*>(expr->constantValue())) {
        return ctx.intConstant(val->value());
    }
    return nullptr;
}

/// Expressions like `[1, 2, 3]` where all elements are constants can be
/// allocated in static memory and then the list expression translates to a
/// memcpy
bool FuncGenContext::genStaticListData(ast::ListExpression const& list,
                                       ir::Alloca* dest) {
    auto* type = cast<sema::ArrayType const*>(list.type().get());
    SC_ASSERT(!type->isDynamic(),
              "Cannot allocate dynamic array in local memory");
    auto* elemType = type->elementType();
    utl::small_vector<ir::Constant*> elems;
    elems.reserve(type->size());
    for (auto* expr: list.elements()) {
        SC_ASSERT(elemType == expr->type().get(), "Invalid type");
        auto* constant = evalConstant(ctx, expr);
        if (!constant) {
            return false;
        }
        elems.push_back(constant);
    }
    auto* irType = ctx.arrayType(typeMap.packed(elemType), type->count());
    auto* value = ctx.arrayConstant(elems, irType);
    auto name = nameFromSourceLoc("listexpr", list.sourceLocation());
    auto* global = mod.makeGlobalConstant(ctx, value, std::move(name));
    callMemcpy(dest, global, irType->size());
    return true;
}

/// General case list expressions like `[computeValue(), parseInt("123")]` must
/// be generated by a sequence of store instructions
void FuncGenContext::genDynamicListData(ast::ListExpression const& list,
                                        ir::Alloca* dest) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = typeMap.packed(arrayType->elementType());
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* elemAddr =
            add<ir::GetElementPointer>(elemType,
                                       dest,
                                       ctx.intConstant(index, 32),
                                       IndexArray{},
                                       utl::strcat("listexpr.elem.", index));
        auto* valAddr = getValue<Packed>(Memory, elem);
        valAddr->replaceAllUsesWith(elemAddr);
    }
}

Value FuncGenContext::getValueImpl(ast::ListExpression const& list) {
    auto* type = cast<sema::ArrayType const*>(list.type().get());
    auto* irElemType = typeMap.packed(type->elementType());
    std::string name = "listexpr";
    auto* array = makeLocalArray(irElemType, type->count(), name);
    if (!genStaticListData(list, array)) {
        genDynamicListData(list, array);
    }
    return Value(name, list.type().get(), { array }, Memory, Unpacked);
}

// Value FuncGenContext::getValueImpl(ast::MoveExpr const& expr) {
//     auto value = getValue(expr.value());
//     auto* ctor = expr.function();
//     if (!ctor) {
//         valueMap.insertArraySizeOf(expr.object(), expr.value()->object());
//         return value;
//     }
//     auto* type = typeMap(expr.type());
//     auto* function = getFunction(ctor);
//     auto* dest =
//         makeLocalVariable(type, utl::strcat(value.get()->name(), ".moved"));
//     add<ir::Call>(function,
//                   ValueArray{ dest, toMemory(value, expr) },
//                   std::string{});
//     Value result(dest, type, Memory);
//     /// TODO: Handle case for dynamic array moved by value
//     if (isFatPointer(&expr)) {
//         auto* addr = add<ir::GetElementPointer>(ctx,
//                                                 arrayPtrType,
//                                                 dest,
//                                                 nullptr,
//                                                 IndexArray{ 1 },
//                                                 "arraysize");
//         valueMap.insertArraySize(expr.object(),
//                                  Value(addr, ctx.intType(64), Memory));
//     }
//     return result;
// }

// Value FuncGenContext::getValueImpl(ast::UniqueExpr const& expr) {
//     auto backItr = std::prev(currentBlock().end());
//     auto* addr = getValue<Memory>(expr.value());
//     SC_ASSERT(
//         isa<ir::Alloca>(addr) || isa<ir::NullPointerConstant>(addr),
//         "We expect the argument to be constructed in local memory and we will
//         rewrite it to heap allocation");
//     ir::Instruction* insertBefore = backItr->next();
//     ir::Callable* alloc = getBuiltin(svm::Builtin::alloc);
//     sema::ObjectType const* baseType =
//         cast<sema::UniquePtrType const&>(*expr.type()).base().get();
//     ir::Value* bytesize = [&]() -> ir::Value* {
//         if (auto* arrayType = dyncast<sema::ArrayType const*>(baseType)) {
//             size_t elemSize = arrayType->elementType()->size();
//             auto* count =
//                 toRegister(valueMap.arraySize(expr.value()->object()), expr);
//             if (auto* countInst = dyncast<ir::Instruction*>(count)) {
//                 insertBefore = countInst->next();
//             }
//             return withBlockCurrent(&currentBlock(),
//                                     ir::BasicBlock::Iterator(insertBefore),
//                                     [&] {
//                 return makeCountToByteSize(count, elemSize);
//             });
//         }
//         else {
//             return ctx.intConstant(baseType->size(), 64);
//         }
//     }();
//     ValueArray args = { bytesize, ctx.intConstant(baseType->align(), 64) };
//     ir::Value* arrayPtr = insert<ir::Call>(insertBefore, alloc, args,
//     "alloc"); ir::Value* ptr = insert<ir::ExtractValue>(insertBefore,
//                                               arrayPtr,
//                                               IndexArray{ 0 },
//                                               "pointer");
//     addr->replaceAllUsesWith(ptr);
//     if (!isFatPointer(&expr)) {
//         return Value(storeToMemory(ptr), ctx.ptrType(), Memory);
//     }
//     else {
//         auto* count =
//             toRegister(valueMap.arraySize(expr.value()->object()), expr);
//         arrayPtr = makeArrayPointer(ptr, count);
//         auto* ptrAddr = storeToMemory(arrayPtr);
//         valueMap.insertArraySize(expr.object(), [=, this] {
//             auto* sizeAddr =
//                 add<ir::GetElementPointer>(ctx,
//                                            arrayPtrType,
//                                            ptrAddr,
//                                            nullptr,
//                                            IndexArray{ 1 },
//                                            "unique.array.size.addr");
//             return Value(sizeAddr, ctx.intType(64), Memory);
//         });
//         return Value(ptrAddr, arrayPtrType, Memory);
//     }
// }

static sema::ObjectType const* stripPtr(sema::ObjectType const* type) {
    if (auto* ptrType = dyncast<sema::PointerType const*>(type)) {
        return ptrType->base().get();
    }
    return type;
}

Value FuncGenContext::getValueImpl(ast::Conversion const& conv) {
    auto* expr = conv.expression();
    Value refConvResult = [&]() -> Value {
        switch (conv.conversion()->valueCatConversion()) {
        case sema::ValueCatConversion::None:
            [[fallthrough]];
        case sema::ValueCatConversion::LValueToRValue:
            return getValue(expr);

        case sema::ValueCatConversion::MaterializeTemporary: {
            auto value = getValue(expr);
            return Value(utl::strcat(value.name(), ".conv"),
                         conv.type().get(),
                         toUnpackedMemory(value),
                         Memory,
                         Unpacked);
        }
        }
        SC_UNREACHABLE();
    }();

    switch (conv.conversion()->objectConversion()) {
        using enum sema::ObjectTypeConversion;
    case None:
        return refConvResult;
        //    case NullPtrToPtr:
        //        if (isFatPointer(&conv)) {
        //            valueMap.insertArraySize(conv.object(),
        //                                     Value(ctx.intConstant(0, 64),
        //                                     Register));
        //        }
        //        return refConvResult;
        //    case NullPtrToUniquePtr: {
        //        Value value(toMemory(refConvResult, conv),
        //                    refConvResult.type(),
        //                    Memory);
        //        if (isFatPointer(&conv)) {
        //            valueMap.insertArraySize(conv.object(),
        //                                     Value(ctx.intConstant(0, 64),
        //                                     Register));
        //        }
        //        return value;
        //    }
        //    case UniquePtrToPtr:
        //        valueMap.insertArraySizeOf(conv.object(),
        //        conv.expression()->object()); return refConvResult;
        //    case Array_FixedToDynamic: {
        //        valueMap.insertArraySizeOf(conv.object(), expr->object());
        //        if (!isa<sema::PointerType>(*expr->type())) {
        //            return refConvResult;
        //        }
        //        auto* count =
        //            toRegister(valueMap.arraySize(conv.expression()->object()),
        //            conv);
        //        auto* arrayPtr =
        //            makeArrayPointer(toRegister(refConvResult, conv), count);
        //        return Value(arrayPtr, Register);
        //    }
        //    case Reinterpret_Array_ToByte:
        //        [[fallthrough]];
        //    case Reinterpret_Array_FromByte: {
        //        auto* fromType =
        //            cast<sema::ArrayType
        //            const*>(stripPtr(expr->type().get()));
        //        auto* toType =
        //            cast<sema::ArrayType const*>(stripPtr(conv.type().get()));
        //        size_t fromElemSize = fromType->elementType()->size();
        //        size_t toElemSize = toType->elementType()->size();
        //        auto data = refConvResult;
        //        if (!toType->isDynamic()) {
        //            SC_ASSERT(!fromType->isDynamic(), "Invalid conversion");
        //            return data;
        //        }
        //        if (fromType->isDynamic()) {
        //            auto fromCount = valueMap.arraySize(expr->object());
        //            if (conv.conversion()->objectConversion() ==
        //                Reinterpret_Array_ToByte)
        //            {
        //                auto* newCount =
        //                    makeCountToByteSize(toRegister(fromCount, conv),
        //                                        fromElemSize);
        //                valueMap.insertArraySize(conv.object(),
        //                                         Value(newCount, Register));
        //            }
        //            else {
        //                auto* newCount =
        //                    add<ir::ArithmeticInst>(toRegister(fromCount,
        //                    conv),
        //                                            ctx.intConstant(toElemSize,
        //                                            64),
        //                                            ir::ArithmeticOperation::SDiv,
        //                                            "reinterpret.count");
        //                valueMap.insertArraySize(conv.object(),
        //                                         Value(newCount, Register));
        //            }
        //        }
        //        else {
        //            switch (conv.conversion()->objectConversion()) {
        //            case Reinterpret_Array_ToByte:
        //                valueMap.insertArraySize(conv.object(),
        //                                         fromType->count() *
        //                                         fromElemSize);
        //                break;
        //            case Reinterpret_Array_FromByte:
        //                valueMap.insertArraySize(conv.object(),
        //                                         fromType->count() /
        //                                         toElemSize);
        //                break;
        //            default:
        //                SC_UNREACHABLE();
        //            }
        //        }
        //        return data;
        //    }
        //    case Reinterpret_Value_ToByteArray: {
        //        auto data = refConvResult;
        //        auto* fromType = stripPtr(expr->type().get());
        //        auto* toType =
        //            cast<sema::ArrayType const*>(stripPtr(conv.type().get()));
        //        if (toType->isDynamic()) {
        //            valueMap.insertArraySize(conv.object(), fromType->size());
        //        }
        //        return data;
        //    }
        //    case Reinterpret_Value_FromByteArray: {
        //        Value data = refConvResult;
        //        auto* fromType =
        //            cast<sema::ArrayType
        //            const*>(stripPtr(expr->type().get()));
        //        auto* toType = stripPtr(conv.type().get());
        //        if (!fromType->isDynamic()) {
        //            SC_ASSERT(fromType->size() == toType->size(), "Size
        //            mismatch");
        //        }
        //        else {
        //            // TODO: Insert runtime check that size is equal
        //        }
        //        if (data.type() == arrayPtrType) {
        //            if (data.isMemory()) {
        //                return Value(data.get(), ctx.ptrType(), Memory);
        //            }
        //            return Value(toThinPointer(data.get()), Register);
        //        }
        //        return data;
        //    }
        //    case Reinterpret_Value: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Bitcast,
        //                                               "reinterpret");
        //        return Value(result, Register);
        //    }
        //    case SS_Trunc:
        //        [[fallthrough]];
        //    case SU_Trunc:
        //        [[fallthrough]];
        //    case US_Trunc:
        //        [[fallthrough]];
        //    case UU_Trunc: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Trunc,
        //                                               "trunc");
        //        return Value(result, Register);
        //    }
        //    case SS_Widen:
        //        [[fallthrough]];
        //    case SU_Widen: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Sext,
        //                                               "sext");
        //        return Value(result, Register);
        //    }
        //    case US_Widen:
        //        [[fallthrough]];
        //    case UU_Widen: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Zext,
        //                                               "zext");
        //        return Value(result, Register);
        //    }
        //    case Float_Trunc: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Ftrunc,
        //                                               "ftrunc");
        //        return Value(result, Register);
        //    }
        //    case Float_Widen: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Fext,
        //                                               "fext");
        //        return Value(result, Register);
        //    }
        //    case SignedToFloat: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::StoF,
        //                                               "stof");
        //        return Value(result, Register);
        //    }
        //    case UnsignedToFloat: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::UtoF,
        //                                               "utof");
        //        return Value(result, Register);
        //    }
        //    case FloatToSigned: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::FtoS,
        //                                               "ftos");
        //        return Value(result, Register);
        //    }
        //    case FloatToUnsigned: {
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(refConvResult, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::FtoU,
        //                                               "ftou");
        //        return Value(result, Register);
        //    }
    }
    SC_UNREACHABLE();
}

// Value FuncGenContext::getValueImpl(ast::UninitTemporary const& temp) {
//     auto* type = [&]() -> ir::Type const* {
//         if (isFatPointer(&temp)) {
//             return arrayPtrType;
//         }
//         return typeMap(temp.type());
//     }();
//     auto* address = makeLocalVariable(type, "anon");
//     return Value(address, type, Memory);
// }

static bool isCopyCtor(sema::Function const& F) {
    auto md = F.getSMFMetadata();
    if (!md) {
        return false;
    }
    using enum sema::SpecialLifetimeFunction;
    return md->isSLF() && md->SLFKind() == CopyConstructor;
}

Value FuncGenContext::getValueImpl(ast::ConstructExpr const& expr) {
    if (auto* type = getDynArrayType(expr.type().get())) {
        SC_UNIMPLEMENTED();
        // return genConstructDynArray(expr, *type);
    }
    /// If we have a constructor we just call that
    if (!expr.isTrivial()) {
        SC_UNIMPLEMENTED();
        //        return genConstructNonTrivial(expr);
    }
    return genConstructTrivial(expr, *expr.type());
}

// Value FuncGenContext::genConstructDynArray(ast::ConstructExpr const& expr,
//                                            sema::ArrayType const& type) {
//     auto* elemType = typeMap(type.elementType());
//     switch (expr.arguments().size()) {
//     case 0:
//         /// No arguments mean we construct an array of size 0
//         valueMap.insertArraySize(expr.object(),
//                                  Value(ctx.intConstant(0, 64), Register));
//         return Value(ctx.nullpointer(), elemType, Memory);
//
//     case 1: {
//         auto* arg = expr.argument(0);
//         if (isa<sema::IntType>(*arg->type())) {
//             /// We construct a dynamic array from a count
//             auto* count = getValue<Register>(arg);
//             auto* storage = makeLocalArray(elemType, count, "dynarray");
//             if (expr.function()) {
//                 add<ir::Call>(getFunction(expr.function()),
//                               ValueArray{ storage, count },
//                               std::string{});
//             }
//             else {
//                 auto* numBytes = makeCountToByteSize(count,
//                 elemType->size()); callMemset(storage, numBytes, 0);
//             }
//             valueMap.insertArraySize(expr.object(), Value(count, Register));
//             return Value(storage, elemType, Memory);
//         }
//         else {
//             /// We construct a dynamic array from another one
//             SC_ASSERT(cast<sema::ArrayType const*>(arg->type().get())
//                               ->elementType() == type.elementType(),
//                       "Copy construction is the only other case");
//             auto argValue = getValue(arg);
//             SC_ASSERT(argValue.isMemory(), "Dynamic array must be in
//             memory"); auto* count =
//             toRegister(valueMap.arraySize(arg->object()), expr); auto*
//             storage = makeLocalArray(elemType, count, "dynarray"); if (auto*
//             ctor = expr.function()) {
//                 auto* irCtor = getFunction(ctor);
//                 ValueArray irArguments = {
//                     storage,
//                     count,
//                     argValue.get(),
//                     count,
//                 };
//                 add<ir::Call>(irCtor, irArguments, std::string{});
//             }
//             else {
//                 auto* numBytes = makeCountToByteSize(count,
//                 elemType->size()); callMemcpy(storage, toMemory(argValue,
//                 expr), numBytes);
//             }
//             valueMap.insertArraySize(expr.object(), Value(count, Register));
//             return Value(storage, elemType, Memory);
//         }
//     }
//     default:
//         SC_UNREACHABLE();
//     }
// }

// Value FuncGenContext::genConstructNonTrivial(ast::ConstructExpr const& expr)
// {
//     SC_EXPECT(!expr.isTrivial());
//     auto* type = typeMap(expr.constructedType());
//     auto* function = getFunction(expr.function());
//     auto CC = getCC(expr.function());
//     /// We generate all argument expressions before we unpack them because
//     /// we may need the array size of the second argument when unpacking the
//     /// first argument
//     auto argValues = expr.arguments() |
//                      transform([&](auto* arg) { return getValue(arg); }) |
//                      ToSmallVector<>;
//     /// If we invoke a copy constructor of a dynamic array type, we give the
//     /// first argument which is an uninit temporary the array size of the
//     /// second argument
//     if (isCopyCtor(*expr.function())) {
//         valueMap.insertArraySizeOf(expr.argument(0)->object(),
//                                    expr.argument(1)->object());
//     }
//     auto irArguments = unpackArguments(CC.arguments(),
//                                        expr.function()->argumentTypes(),
//                                        expr.arguments() | ToSmallVector<>,
//                                        argValues);
//     SC_ASSERT(!irArguments.empty(), "Must have at least the object
//     argument"); add<ir::Call>(function, irArguments, std::string{});
//
//     SC_ASSERT(!getDynArrayType(expr.type().get()), "");
//
//     /// For unique pointers we lazily load the array size from memory
//     if (isFatPointer(&expr)) {
//         valueMap.insertArraySize(expr.object(), [=, this] {
//             auto* size = add<ir::GetElementPointer>(ctx,
//                                                     arrayPtrType,
//                                                     irArguments.front(),
//                                                     nullptr,
//                                                     IndexArray{ 1 },
//                                                     "arraysize");
//             return Value(size, ctx.intType(64), Memory);
//         });
//     }
//     return Value(irArguments.front(), type, Memory);
// }

Value FuncGenContext::genConstructTrivial(ast::ConstructExpr const& expr,
                                          sema::Type const& type) {
    return visit(type, [&](auto& type) SC_NODEBUG {
        return genConstructTrivialImpl(expr, type);
    });
}

Value FuncGenContext::genConstructTrivialImpl(ast::ConstructExpr const& expr,
                                              sema::ObjectType const& type) {
    if (expr.arguments().size() == 1) {
        auto value = getValue(expr.arguments().front());
        return Value(value.name(),
                     &type,
                     { toPackedRegister(value) },
                     Register,
                     Packed);
    }
    SC_ASSERT(expr.arguments().empty(), "Must be one element or empty");
    // clang-format off
    auto* value = SC_MATCH (type) {
        [&](sema::BoolType const&) {
            /// Bools are default initialized to `false`
            return ctx.boolConstant(false);
        },
        [&](sema::ByteType const&) {
            return ctx.intConstant(0, 8);
        },
        [&](sema::IntType const& type) {
            return ctx.intConstant(0, type.bitwidth());
        },
        [&](sema::FloatType const& type) {
            return ctx.floatConstant(0, type.bitwidth());
        },
        [&](sema::NullPtrType const&) {
            return ctx.nullpointer();
        },
        [&](sema::RawPtrType const&) {
            return ctx.nullpointer();
        },
        [&](sema::ObjectType const&) -> ir::Value* {
            SC_UNREACHABLE();
        },
    }; // clang-format on
    return Value("tmp", &type, { value }, Register, Packed);
}

Value FuncGenContext::genConstructTrivialImpl(ast::ConstructExpr const& expr,
                                              sema::StructType const& type) {
    SC_UNIMPLEMENTED();
    //    if (expr.arguments().empty()) {
    //        auto* irType = typeMap(&type);
    //        auto* addr = makeLocalVariable(irType, "tmp");
    //        auto* call = callMemset(addr, irType->size(), 0);
    //        addSourceLoc(call, expr);
    //        return Value(addr, irType, Memory);
    //    }
    //    else if (expr.arguments().size() == 1 &&
    //             expr.arguments().front()->type().get() == expr.type().get())
    //    {
    //        auto* arg = expr.arguments().front();
    //        auto* value = getValue<Register>(arg);
    //        valueMap.insertArraySizeOf(expr.object(), arg->object());
    //        return Value(value, Register);
    //    }
    //    else {
    //        ir::Value* aggregate = ctx.undef(typeMap(expr.type()));
    //        size_t index = 0;
    //        for (auto* arg: expr.arguments()) {
    //            auto* member = getValue<Register>(arg);
    //            auto* inst = add<ir::InsertValue>(aggregate,
    //                                              member,
    //                                              IndexArray{ index++ },
    //                                              "aggregate");
    //            addSourceLoc(inst, *arg);
    //            aggregate = inst;
    //            if (isFatPointer(arg->type().get())) {
    //                auto size = valueMap.arraySize(arg->object());
    //                auto* inst = add<ir::InsertValue>(aggregate,
    //                                                  toRegister(size, *arg),
    //                                                  IndexArray{ index++ },
    //                                                  "aggregate");
    //                addSourceLoc(inst, *arg);
    //                aggregate = inst;
    //            }
    //        }
    //        return Value(aggregate, Register);
    //    }
}

Value FuncGenContext::genConstructTrivialImpl(ast::ConstructExpr const& expr,
                                              sema::ArrayType const& type) {
    SC_ASSERT(!type.isDynamic(), "Dynamic arrays have their own function");
    std::string name = "tmp.array";
    auto* irElemType = typeMap.packed(type.elementType());
    switch (expr.arguments().size()) {
    case 0: {
        auto* addr = makeLocalArray(irElemType,
                                    type.count(),
                                    utl::strcat(name, ".addr"));
        callMemset(addr, irElemType->size() * type.count(), 0);
        return Value(name, &type, { addr }, Memory, Unpacked);
    }
    case 1: {
        auto* arg = expr.arguments().front();
        if (type.size() <= PreferredMaxRegisterValueSize) {
            auto* value = getValue<Unpacked>(Register, arg).front();
            return Value(name, &type, { value }, Register, Unpacked);
        }
        else {
            auto source = getValue(arg);
            SC_ASSERT(
                source.isMemory(),
                "Should be in memory if size is beyond preferred register size limit");
            auto* addr = makeLocalArray(irElemType,
                                        type.count(),
                                        utl::strcat(name, ".addr"));
            callMemcpy(addr, source.get(0), irElemType->size() * type.count());
            return Value(name, &type, { addr }, Memory, Packed);
        }
    }
    default:
        SC_UNREACHABLE();
    }
}

// Value FuncGenContext::getValueImpl(ast::NonTrivAssignExpr const& expr) {
//     /// If the values are different, we  call the destructor of LHS and the
//     copy
//     /// or move constructor of LHS with RHS as argument. If the values are
//     the
//     /// same we do nothing
//     auto* dest = getValue<Memory>(expr.dest());
//     auto* source = getValue<Memory>(expr.source());
//     /// We branch here because the values might be the same.
//     auto* addrEq = add<ir::CompareInst>(dest,
//                                         source,
//                                         ir::CompareMode::Unsigned,
//                                         ir::CompareOperation::NotEqual,
//                                         "addr.eq");
//     auto* assignBlock = newBlock("assign");
//     auto* endBlock = newBlock("assign.end");
//     add<ir::Branch>(addrEq, assignBlock, endBlock);
//     add(assignBlock);
//     callDtor(expr.dest()->object(), expr.dtor(), expr);
//     auto* function = getFunction(expr.ctor());
//     add<ir::Call>(function, ValueArray{ dest, source }, std::string{});
//     add<ir::Goto>(endBlock);
//     add(endBlock);
//     return Value();
// }

/// MARK: - General utilities

void FuncGenContext::callDtor(sema::Object const* object,
                              sema::Function const* dtor,
                              ast::ASTNode const& sourceNode) {
    SC_UNIMPLEMENTED();
    //    auto* function = getFunction(dtor);
    //    auto* value = toMemory(valueMap(object), sourceNode);
    //    add<ir::Call>(function, ValueArray{ value }, std::string{});
}

void FuncGenContext::emitDtorCalls(sema::DtorStack const& dtorStack,
                                   ast::ASTNode const& sourceNode) {
    for (auto call: dtorStack) {
        callDtor(call.object, call.destructor, sourceNode);
    }
}

// ir::Value* FuncGenContext::toRegister(Value value,
//                                       ast::ASTNode const& sourceNode) {
//     if (isa<ir::RecordType>(value.type())) {
//         auto* semaType = typeMap(value.type());
//         SC_ASSERT(!semaType || semaType->hasTrivialLifetime(),
//                   "We can only have trivial lifetime types in registers");
//     }
//     switch (value.location()) {
//     case Register:
//         return value.get();
//     case Memory:
//         auto* load = add<ir::Load>(value.get(),
//                                    value.type(),
//                                    std::string(value.get()->name()));
//         addSourceLoc(load, sourceNode);
//         return load;
//     }
//     SC_UNREACHABLE();
// }

// ir::Value* FuncGenContext::toMemory(Value value, ast::ASTNode const&) {
//     switch (value.location()) {
//     case Register:
//         return storeToMemory(value.get());
//
//     case Memory:
//         return value.get();
//     }
//     SC_UNREACHABLE();
// }

void FuncGenContext::addSourceLoc(ir::Instruction* inst,
                                  ast::ASTNode const& sourceNode) {
    if (!config.generateDebugSymbols) {
        return;
    }
    inst->setMetadata(sourceNode.sourceRange().begin());
}
