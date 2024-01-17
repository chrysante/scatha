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
    //void generateImpl(ast::ImportStatement const&);
    void generateImpl(ast::CompoundStatement const&);
    void generateImpl(ast::FunctionDefinition const&);
    void generateParameter(ast::ParameterDeclaration const* paramDecl,
                           PassingConvention pc,
                           List<ir::Parameter>::iterator& paramItr);
    //void generateImpl(ast::VariableDeclaration const&);
    void generateImpl(ast::ExpressionStatement const&);
    //void generateImpl(ast::EmptyStatement const&) {}
    void generateImpl(ast::ReturnStatement const&);
    //void generateImpl(ast::IfStatement const&);
    //void generateImpl(ast::LoopStatement const&);
    //void generateImpl(ast::JumpStatement const&);

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

    template <ValueLocation Loc>
    SC_NODEBUG ir::Value* getValue(ast::Expression const* expr);

    Value getValueImpl(ast::Expression const&) { SC_UNREACHABLE(); }
    Value getValueImpl(ast::Identifier const&);
    //Value getValueImpl(ast::Literal const&);
    //Value getValueImpl(ast::UnaryExpression const&);
    //Value getValueImpl(ast::BinaryExpression const&);
    Value getValueImpl(ast::MemberAccess const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Variable const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Property const&);
    Value genMemberAccess(ast::MemberAccess const&, sema::Temporary const&) {
        SC_UNREACHABLE();
    }
    Value getValueImpl(ast::DereferenceExpression const&);
    //Value getValueImpl(ast::AddressOfExpression const&);
    //Value getValueImpl(ast::Conditional const&);
    //Value getValueImpl(ast::FunctionCall const&);
    //Value getValueImpl(ast::Subscript const&);
    //Value getValueImpl(ast::SubscriptSlice const&);
    //Value getValueImpl(ast::ListExpression const&);
    //Value getValueImpl(ast::MoveExpr const&);
    //Value getValueImpl(ast::UniqueExpr const&);
    //Value getValueImpl(ast::Conversion const&);
    //Value getValueImpl(ast::UninitTemporary const&);
    Value getValueImpl(ast::ConstructExpr const&);
    //Value genConstructDynArray(ast::ConstructExpr const&,
    //                           sema::ArrayType const&);
    //Value genConstructNonTrivial(ast::ConstructExpr const&);
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
    //Value getValueImpl(ast::NonTrivAssignExpr const&);

    /// # Expression specific utilities

    /// Generates function call argument(s) for the expression \p expr
    /// In particalur this functions decides depending on the passing
    /// convention\p PC if the argument is passed by register or in memory and
    /// loads or stores it accordingly of necessary. This function also emits
    /// two call arguments for dynamic array pointers.
    /// If \p value is null this function will generate the code for the
    /// argument.
    void unpackArgument(PassingConvention const& PC,
                        sema::Type const* paramType,
                        ast::Expression const* expr,
                        Value value,
                        utl::vector<ir::Value*>& irArgsOut);
    /// Calls `unpackArgument()` in a loop and returns the unpacked IR argument
    /// values
    utl::small_vector<ir::Value*> unpackArguments(
        std::span<PassingConvention const> PCs,
        std::span<sema::Type const* const> paramTypes,
        std::span<ast::Expression const* const> arguments,
        std::span<Value> argValues);
    ir::ArrayType const* getListType(ast::ListExpression const& list);
    bool genStaticListData(ast::ListExpression const& list, ir::Alloca* dest);
    void genDynamicListData(ast::ListExpression const& list, ir::Alloca* dest);

    /// # General utilities

    /// If the value \p value is already in a register, returns that.
    /// Otherwise loads the value from memory and returns the `load` instruction
    ir::Value* toRegister(Value value, ast::ASTNode const& sourceNode);

    /// If the value \p value is in memory, returns the address.
    /// Otherwise allocates stack memory, stores the value and returns the
    /// address
    ir::Value* toMemory(Value value, ast::ASTNode const& sourceNode);

    /// \Returns `toRegister(value)` or `toMemory(value)` depending on \p
    /// location
    ir::Value* toValueLocation(ValueLocation location,
                               Value value,
                               ast::ASTNode const& sourceNode);

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

//void FuncGenContext::generateImpl(ast::ImportStatement const&) {
//    /// No-op
//}

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
    List<ir::Parameter>::iterator& irParamItr) 
{
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
        /// Reference parameters are special: We don't store them to memory because they cannot be reassigned
        if (isa<sema::ReferenceType>(semaParam->type())) {
            valueMap.insert(semaParam, Value(name,
                                             paramType,
                                             params, Memory, Unpacked));
        }
        else if (params.size() == 1) {
            auto* val = params.front();
            auto* mem = storeToMemory(val, name);
            valueMap.insert(semaParam, Value(name, paramType, ValueArray{mem}, Memory, Unpacked));
        }
        else {
            auto* packedVal = packValues(params, name);
            auto* mem = storeToMemory(packedVal, name);
            valueMap.insert(semaParam, Value(name, paramType, {mem}, Memory, Packed));
        }
        break;
    }

    case Memory:
        SC_ASSERT(params.size() == 1, "");
        valueMap.insert(semaParam, Value(name, paramType, params, Memory, Unpacked));
        break;
    }
}

//void FuncGenContext::generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
//                                              sema::Object const* initObject) {
//    if (!isFatPointer(varDecl->type())) {
//        return;
//    }
//    auto size = valueMap.arraySize(initObject);
//    if (isa<sema::ReferenceType>(varDecl->type())) {
//        valueMap.insertArraySize(varDecl->variable(),
//                                 Value(toRegister(size, *varDecl), Register));
//    }
//    else {
//        auto* newSize = storeToMemory(toRegister(size, *varDecl),
//                                      utl::strcat(varDecl->name(), ".size"));
//        valueMap.insertArraySize(varDecl->variable(),
//                                 Value(newSize, size.type(), Memory));
//    }
//}

//void FuncGenContext::generateImpl(ast::VariableDeclaration const& varDecl) {
//    auto dtorStack = varDecl.dtorStack();
//    std::string name = std::string(varDecl.name());
//    auto* initExpr = varDecl.initExpr();
//    if (isa<sema::ReferenceType>(varDecl.type())) {
//        SC_ASSERT(initExpr, "Reference must be initialized");
//        auto value = getValue(initExpr);
//        valueMap.insert(varDecl.variable(), value);
//        generateVarDeclArraySize(&varDecl, initExpr->object());
//    }
//    else if (initExpr) {
//        /// If we have non-trivial types sema ensures that we always have an
//        /// init-expr in memory
//        auto value = getValue(initExpr);
//        auto* address = toMemory(value, varDecl);
//        address->setName(name);
//        valueMap.insert(varDecl.variable(),
//                        Value(address, value.type(), Memory));
//        generateVarDeclArraySize(&varDecl, initExpr->object());
//    }
//    else {
//        /// Here we could initialize the memory
//        auto* type = typeMap(varDecl.type());
//        auto* address = makeLocalVariable(type, name);
//        valueMap.insert(varDecl.variable(), Value(address, type, Memory));
//        generateVarDeclArraySize(&varDecl, nullptr);
//    }
//    emitDtorCalls(dtorStack, varDecl);
//}

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
        auto* value = destLocation == Memory ? 
            toPackedMemory(retval) :
            toPackedRegister(retval);
        emitDtorCalls(retStmt.dtorStack(), retStmt);
        add<ir::Return>(value);
        return;
    }
    case Memory: {
        SC_UNIMPLEMENTED();
//        auto* retvalDest = &irFn.parameters().front();
//        if (retval.isMemory()) {
//            if (auto* allocaInst = dyncast<ir::Alloca*>(retval.get())) {
//                allocaInst->replaceAllUsesWith(retvalDest);
//            }
//            else {
//                callMemcpy(retvalDest,
//                           toMemory(retval, retStmt),
//                           retval.type()->size());
//            }
//        }
//        else {
//            add<ir::Store>(retvalDest, toRegister(retval, retStmt));
//        }
//        actualRetval = ctx.voidValue();
//        break;
    }
    }
}

//void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
//    auto* condition = getValue<Register>(stmt.condition());
//    emitDtorCalls(stmt.dtorStack(), stmt);
//    auto* thenBlock = newBlock("if.then");
//    auto* elseBlock = stmt.elseBlock() ? newBlock("if.else") : nullptr;
//    auto* endBlock = newBlock("if.end");
//    add<ir::Branch>(condition, thenBlock, elseBlock ? elseBlock : endBlock);
//    add(thenBlock);
//    generate(*stmt.thenBlock());
//    add<ir::Goto>(endBlock);
//    if (stmt.elseBlock()) {
//        add(elseBlock);
//        generate(*stmt.elseBlock());
//        add<ir::Goto>(endBlock);
//    }
//    add(endBlock);
//}

//void FuncGenContext::generateImpl(ast::LoopStatement const& loopStmt) {
//    switch (loopStmt.kind()) {
//    case ast::LoopKind::For: {
//        auto* loopHeader = newBlock("loop.header");
//        auto* loopBody = newBlock("loop.body");
//        auto* loopInc = newBlock("loop.inc");
//        auto* loopEnd = newBlock("loop.end");
//        generate(*loopStmt.varDecl());
//        add<ir::Goto>(loopHeader);
//
//        /// Header
//        add(loopHeader);
//        auto* condition = getValue<Register>(loopStmt.condition());
//        emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//        add<ir::Branch>(condition, loopBody, loopEnd);
//        loopStack.push({ .header = loopHeader,
//                         .body = loopBody,
//                         .inc = loopInc,
//                         .end = loopEnd });
//
//        /// Body
//        add(loopBody);
//        generate(*loopStmt.block());
//        add<ir::Goto>(loopInc);
//
//        /// Inc
//        add(loopInc);
//        getValue(loopStmt.increment());
//        emitDtorCalls(loopStmt.incrementDtorStack(), loopStmt);
//        add<ir::Goto>(loopHeader);
//
//        /// End
//        add(loopEnd);
//        loopStack.pop();
//        break;
//    }
//
//    case ast::LoopKind::While: {
//        auto* loopHeader = newBlock("loop.header");
//        auto* loopBody = newBlock("loop.body");
//        auto* loopEnd = newBlock("loop.end");
//        add<ir::Goto>(loopHeader);
//
//        /// Header
//        add(loopHeader);
//        auto* condition = getValue<Register>(loopStmt.condition());
//        emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//        add<ir::Branch>(condition, loopBody, loopEnd);
//        loopStack.push(
//            { .header = loopHeader, .body = loopBody, .end = loopEnd });
//
//        /// Body
//        add(loopBody);
//        generate(*loopStmt.block());
//        add<ir::Goto>(loopHeader);
//
//        /// End
//        add(loopEnd);
//        loopStack.pop();
//        break;
//    }
//
//    case ast::LoopKind::DoWhile: {
//        auto* loopBody = newBlock("loop.body");
//        auto* loopFooter = newBlock("loop.footer");
//        auto* loopEnd = newBlock("loop.end");
//        add<ir::Goto>(loopBody);
//        loopStack.push(
//            { .header = loopFooter, .body = loopBody, .end = loopEnd });
//
//        /// Body
//        add(loopBody);
//        generate(*loopStmt.block());
//        add<ir::Goto>(loopFooter);
//
//        /// Footer
//        add(loopFooter);
//        auto* condition = getValue<Register>(loopStmt.condition());
//        emitDtorCalls(loopStmt.conditionDtorStack(), loopStmt);
//        add<ir::Branch>(condition, loopBody, loopEnd);
//
//        /// End
//        add(loopEnd);
//        loopStack.pop();
//        break;
//    }
//    }
//    emitDtorCalls(loopStmt.dtorStack(), loopStmt);
//}

//void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
//    emitDtorCalls(jump.dtorStack(), jump);
//    auto* dest = [&] {
//        auto& currentLoop = loopStack.top();
//        switch (jump.kind()) {
//        case ast::JumpStatement::Break:
//            return currentLoop.end;
//        case ast::JumpStatement::Continue:
//            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
//        }
//        SC_UNREACHABLE();
//    }();
//    add<ir::Goto>(dest);
//}

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

template <ValueLocation Loc>
ir::Value* FuncGenContext::getValue(ast::Expression const* expr) {
    return toValueLocation(Loc, getValue(expr), *expr);
}

Value FuncGenContext::getValueImpl(ast::Identifier const& id) {
    return valueMap(id.object());
}

//Value FuncGenContext::getValueImpl(ast::Literal const& lit) {
//    using enum ast::LiteralKind;
//    switch (lit.kind()) {
//    case Integer:
//        return Value(ctx.intConstant(lit.value<APInt>()), Register);
//    case Boolean:
//        return Value(ctx.intConstant(lit.value<APInt>()), Register);
//    case FloatingPoint:
//        return Value(ctx.floatConstant(lit.value<APFloat>()), Register);
//    case Null:
//        return Value(ctx.nullpointer(), Register);
//    case This:
//        return valueMap(lit.object());
//    case String: {
//        auto const& text = lit.value<std::string>();
//        auto name = nameFromSourceLoc("string", lit.sourceLocation());
//        auto* data = ctx.stringLiteral(text);
//        auto* global = mod.makeGlobalConstant(ctx, data, std::move(name));
//        auto value = Value(global, data->type(), Memory);
//        valueMap.insertArraySize(lit.object(), text.size());
//        return value;
//    }
//    case Char:
//        return Value(ctx.intConstant(lit.value<APInt>()), Register);
//    }
//    SC_UNREACHABLE();
//}

//Value FuncGenContext::getValueImpl(ast::UnaryExpression const& expr) {
//    using enum ast::UnaryOperator;
//    switch (expr.operation()) {
//    case Increment:
//        [[fallthrough]];
//    case Decrement: {
//        Value operand = getValue(expr.operand());
//        SC_ASSERT(operand.isMemory(),
//                  "Operand must be in memory to be modified");
//        ir::Value* opAddr = toMemory(operand, expr);
//        ir::Type const* operandType = typeMap(expr.operand()->type());
//        ir::Value* operandValue = toRegister(operand, expr);
//        auto* newValue =
//            add<ir::ArithmeticInst>(operandValue,
//                                    ctx.arithmeticConstant(1, operandType),
//                                    expr.operation() == Increment ?
//                                        ir::ArithmeticOperation::Add :
//                                        ir::ArithmeticOperation::Sub,
//                                    utl::strcat(expr.operation(), ".res"));
//        add<ir::Store>(opAddr, newValue);
//        switch (expr.notation()) {
//        case ast::UnaryOperatorNotation::Prefix:
//            return operand;
//        case ast::UnaryOperatorNotation::Postfix:
//            return Value(operandValue, Register);
//        }
//    }
//
//    case ast::UnaryOperator::Promotion:
//        return getValue(expr.operand());
//
//    case ast::UnaryOperator::Negation: {
//        auto* operand = getValue<Register>(expr.operand());
//        auto operation = isa<sema::IntType>(expr.operand()->type().get()) ?
//                             ir::ArithmeticOperation::Sub :
//                             ir::ArithmeticOperation::FSub;
//        auto* newValue =
//            add<ir::ArithmeticInst>(ctx.arithmeticConstant(0, operand->type()),
//                                    operand,
//                                    operation,
//                                    "negated");
//        return Value(newValue, Register);
//    }
//
//    default:
//        auto* operand = getValue<Register>(expr.operand());
//        auto* newValue =
//            add<ir::UnaryArithmeticInst>(operand,
//                                         mapUnaryOp(expr.operation()),
//                                         "expr");
//        return Value(newValue, Register);
//    }
//}

static std::string getResultName(ast::BinaryOperator op) {
    using enum ast::BinaryOperator;
    switch (op) {
    case Multiplication:
        return "prod";
    case Division:
        return "quot";
    case Remainder:
        return "rem";
    case Addition:
        return "sum";
    case Subtraction:
        return "diff";
    case LeftShift:
        return "lshift";
    case RightShift:
        return "rshift";
    case Less:
        return "ls";
    case LessEq:
        return "lseq";
    case Greater:
        return "grt";
    case GreaterEq:
        return "grteq";
    case Equals:
        return "eq";
    case NotEquals:
        return "neq";
    case BitwiseAnd:
        return "and";
    case BitwiseXOr:
        return "xor";
    case BitwiseOr:
        return "or";
    case LogicalAnd:
        return "land";
    case LogicalOr:
        return "lor";
    case Assignment:
        return "?";
    case AddAssignment:
        return "sum";
    case SubAssignment:
        return "diff";
    case MulAssignment:
        return "prod";
    case DivAssignment:
        return "quot";
    case RemAssignment:
        return "rem";
    case LSAssignment:
        return "lshift";
    case RSAssignment:
        return "rshift";
    case AndAssignment:
        return "and";
    case OrAssignment:
        return "or";
    case XOrAssignment:
        return "xor";
    case Comma:
        return "?";
    }
    SC_UNREACHABLE();
}

//Value FuncGenContext::getValueImpl(ast::BinaryExpression const& expr) {
//    auto* type = expr.lhs()->type().get();
//    auto resName = getResultName(expr.operation());
//    using enum ast::BinaryOperator;
//    switch (expr.operation()) {
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
//        auto* lhs = getValue<Register>(expr.lhs());
//        auto* rhs = getValue<Register>(expr.rhs());
//        auto operation = mapArithmeticOp(type, expr.operation());
//        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, resName);
//        return Value(result, Register);
//    }
//
//    case LogicalAnd:
//        [[fallthrough]];
//    case LogicalOr: {
//        ir::Value* const lhs = getValue<Register>(expr.lhs());
//        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
//        auto* startBlock = &currentBlock();
//        auto* rhsBlock = newBlock("log.rhs");
//        auto* endBlock = newBlock("log.end");
//        if (expr.operation() == LogicalAnd) {
//            add<ir::Branch>(lhs, rhsBlock, endBlock);
//        }
//        else {
//            add<ir::Branch>(lhs, endBlock, rhsBlock);
//        }
//
//        add(rhsBlock);
//        auto* rhs = getValue<Register>(expr.rhs());
//        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
//        add<ir::Goto>(endBlock);
//        add(endBlock);
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
//    }
//
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
//        auto* result =
//            add<ir::CompareInst>(toThinPointer(getValue<Register>(expr.lhs())),
//                                 toThinPointer(getValue<Register>(expr.rhs())),
//                                 mapCompareMode(type),
//                                 mapCompareOp(expr.operation()),
//                                 resName);
//        return Value(result, Register);
//    }
//
//    case Comma:
//        getValue(expr.lhs());
//        return getValue(expr.rhs());
//
//    case Assignment: {
//        auto lhs = getValue<Memory>(expr.lhs());
//        auto rhs = getValue<Register>(expr.rhs());
//        add<ir::Store>(lhs, rhs);
//        /// It is never a reference because expressions don't have reference
//        /// type
//        if (isFatPointer(expr.lhs())) {
//            auto lhsSize = valueMap.arraySize(expr.lhs()->object());
//            SC_ASSERT(lhsSize.location() == Memory,
//                      "Must be in memory to reassign");
//            auto rhsSize = valueMap.arraySize(expr.rhs()->object());
//            add<ir::Store>(lhsSize.get(), toRegister(rhsSize, expr));
//        }
//        return Value();
//    }
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
//        auto lhs = getValue(expr.lhs());
//        SC_ASSERT(lhs.isMemory(), "");
//        auto rhs = getValue<Register>(expr.rhs());
//        SC_ASSERT(type == expr.rhs()->type().get(), "");
//        auto operation = mapArithmeticAssignOp(type, expr.operation());
//        rhs = add<ir::ArithmeticInst>(toRegister(lhs, expr),
//                                      rhs,
//                                      operation,
//                                      resName);
//        add<ir::Store>(lhs.get(), rhs);
//        return Value();
//    }
//    }
//    SC_UNREACHABLE();
//}

Value FuncGenContext::getValueImpl(ast::MemberAccess const& expr) {
    return visit(*expr.member()->object(),
                 [&](auto& obj) { return genMemberAccess(expr, obj); });
}

Value FuncGenContext::genMemberAccess(ast::MemberAccess const& expr,
                                      sema::Variable const& var) {
//    Value base = getValue(expr.accessed());
//    auto const& metaData = typeMap.metaData(expr.accessed()->type().get());
//    size_t const irIndex = metaData.indexMap[var.index()];
//    switch (base.location()) {
//    case Register: {
//        if (isFatPointer(&expr)) {
//            valueMap.insertArraySize(expr.object(), [=, this] {
//                auto* result = add<ir::ExtractValue>(base.get(),
//                                                     IndexArray{ irIndex + 1 },
//                                                     "mem.acc.size");
//                return Value(result, Register);
//            });
//        }
//        auto* result =
//            add<ir::ExtractValue>(base.get(), IndexArray{ irIndex }, "mem.acc");
//        addSourceLoc(result, expr);
//        return Value(result, Register);
//    }
//    case Memory: {
//        if (isFatPointer(&expr)) {
//            valueMap.insertArraySize(expr.object(), [=, this] {
//                auto* result =
//                    add<ir::GetElementPointer>(base.type(),
//                                               base.get(),
//                                               ctx.intConstant(0, 64),
//                                               IndexArray{ irIndex + 1 },
//                                               "mem.acc.size");
//                return Value(result, ctx.intType(64), Memory);
//            });
//        }
//        auto* result = add<ir::GetElementPointer>(base.type(),
//                                                  base.get(),
//                                                  ctx.intConstant(0, 64),
//                                                  IndexArray{ irIndex },
//                                                  "mem.acc");
//        addSourceLoc(result, expr);
//        auto* accessedType = typeMap(var.type());
//        return Value(result, accessedType, Memory);
//    }
//    }
    SC_UNREACHABLE();
}

Value FuncGenContext::genMemberAccess(ast::MemberAccess const& expr,
                                      sema::Property const& prop) {
    using enum sema::PropertyKind;
    switch (prop.kind()) {
    case ArraySize: {
        return getArraySize(expr.accessed()->type().get(), getValue(expr.accessed()));
    }
    case ArrayEmpty: {
        SC_UNIMPLEMENTED();
//        auto* arrayType =
//            cast<sema::ArrayType const*>(expr.accessed()->type().get());
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
//            return Value(ctx.boolConstant(arrayType->count() == 0), Register);
//        }
    }
    case ArrayFront:
        [[fallthrough]];
    case ArrayBack: {
        SC_UNIMPLEMENTED();
//        // TODO: Check that array is not empty
//        auto* arrayType =
//            cast<sema::ArrayType const*>(expr.accessed()->type().get());
//        auto array = getValue(expr.accessed());
//        switch (array.location()) {
//        case Register: {
//            SC_ASSERT(!arrayType->isDynamic(),
//                      "Can't have dynamic array in register");
//            size_t index = prop.kind() == ArrayFront ? 0 :
//                                                       arrayType->count() - 1;
//
//            if (isFatPointer(arrayType->elementType())) {
//                auto* data = add<ir::ExtractValue>(array.get(),
//                                                   IndexArray{ index, 0 },
//                                                   "array.front.data");
//                auto* size = add<ir::ExtractValue>(array.get(),
//                                                   IndexArray{ index, 1 },
//                                                   "array.front.size");
//                valueMap.insertArraySize(expr.object(), Value(size, Register));
//                return Value(data, Register);
//            }
//            else {
//                auto* elem = add<ir::ExtractValue>(array.get(),
//                                                   IndexArray{ index },
//                                                   "array.front");
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
//                    return ctx.intConstant(arrayType->count() - 1, 64);
//                }
//                auto count = valueMap.arraySize(expr.accessed()->object());
//                return add<ir::ArithmeticInst>(toRegister(count, expr),
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
//                                                        IndexArray{ 0 },
//                                                        "array.front.data");
//                auto* size = add<ir::GetElementPointer>(arrayView,
//                                                        array.get(),
//                                                        index,
//                                                        IndexArray{ 1 },
//                                                        "array.front.size");
//                valueMap.insertArraySize(expr.object(),
//                                         Value(size, irSizeType, Memory));
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
                 Memory, Unpacked);
}

//Value FuncGenContext::getValueImpl(ast::AddressOfExpression const& expr) {
//    auto* ptr = getValue<Memory>(expr.referred());
//    valueMap.insertArraySizeOf(expr.object(), expr.referred()->object());
//    if (!isFatPointer(&expr)) {
//        return Value(ptr, Register);
//    }
//    auto* count =
//        toRegister(valueMap.arraySize(expr.referred()->object()), expr);
//    auto* arrayPtr = makeArrayPointer(ptr, count);
//    return Value(arrayPtr, Register);
//}

//Value FuncGenContext::getValueImpl(ast::Conditional const& condExpr) {
//    auto* cond = getValue<Register>(condExpr.condition());
//    auto* thenBlock = newBlock("cond.then");
//    auto* elseBlock = newBlock("cond.else");
//    auto* endBlock = newBlock("cond.end");
//
//    ir::Value* thenSize = nullptr;
//    ir::Value* elseSize = nullptr;
//
//    add<ir::Branch>(cond, thenBlock, elseBlock);
//
//    /// Generate then block.
//    add(thenBlock);
//    auto thenVal = getValue(condExpr.thenExpr());
//    if (isFatPointer(&condExpr)) {
//        thenSize = toRegister(valueMap.arraySize(condExpr.thenExpr()->object()),
//                              condExpr);
//    }
//    thenBlock = &currentBlock(); /// Nested `?:` operands etc. may have changed
//                                 /// `currentBlock`
//    add<ir::Goto>(endBlock);
//
//    /// Generate else block.
//    add(elseBlock);
//    auto elseVal = getValue(condExpr.elseExpr());
//    if (isFatPointer(&condExpr)) {
//        elseSize = toRegister(valueMap.arraySize(condExpr.elseExpr()->object()),
//                              condExpr);
//    }
//    elseBlock = &currentBlock();
//    add<ir::Goto>(endBlock);
//
//    /// Generate end block.
//    add(endBlock);
//    auto loc = commonLocation(thenVal.location(), elseVal.location());
//    auto* thenValResolved =
//        withBlockCurrent(thenBlock, std::prev(thenBlock->end()), [&] {
//        return toValueLocation(loc, thenVal, condExpr);
//    });
//    auto* elseValResolved =
//        withBlockCurrent(elseBlock, std::prev(elseBlock->end()), [&] {
//        return toValueLocation(loc, elseVal, condExpr);
//    });
//    std::array phiArgs = { ir::PhiMapping{ thenBlock, thenValResolved },
//                           ir::PhiMapping{ elseBlock, elseValResolved } };
//    auto* phi = add<ir::Phi>(phiArgs, "cond");
//    Value value(phi, typeMap(condExpr.type()), loc);
//    if (isFatPointer(&condExpr)) {
//        std::array phiArgs = { ir::PhiMapping{ thenBlock, thenSize },
//                               ir::PhiMapping{ elseBlock, elseSize } };
//        auto* sizePhi = add<ir::Phi>(phiArgs, "cond");
//        Value size(sizePhi, Register);
//        valueMap.insertArraySize(condExpr.object(), size);
//    }
//    return value;
//}

//Value FuncGenContext::getValueImpl(ast::FunctionCall const& call) {
//    ir::Callable* function = getFunction(call.function());
//    auto CC = getCC(call.function());
//    auto const retvalLocation = CC.returnValue().location();
//    utl::small_vector<ir::Value*> irArguments;
//    if (retvalLocation == Memory) {
//        auto* returnType = typeMap(call.function()->returnType());
//        irArguments.push_back(makeLocalVariable(returnType, "retval"));
//    }
//    for (auto [PC, paramType, argExpr]:
//         ranges::views::zip(CC.arguments(),
//                            call.function()->argumentTypes(),
//                            call.arguments()))
//    {
//        unpackArgument(PC, paramType, argExpr, {}, irArguments);
//    }
//    bool callHasName = !isa<ir::VoidType>(function->returnType());
//    std::string name = callHasName ? "call.result" : std::string{};
//    auto* inst = add<ir::Call>(function, irArguments, std::move(name));
//    auto semaRetType = call.function()->returnType();
//    Value value;
//    switch (retvalLocation) {
//    case Register: {
//        auto* refType = dyncast<sema::ReferenceType const*>(semaRetType);
//        if (isFatPointer(semaRetType)) {
//            if (refType) {
//                value = Value(inst, typeMap(refType->base()), Memory);
//            }
//            else {
//                value = Value(inst, Register);
//            }
//            auto* size = add<ir::ExtractValue>(inst, IndexArray{ 1 }, "size");
//            valueMap.insertArraySize(call.object(), Value(size, Register));
//        }
//        else {
//            if (refType) {
//                value = Value(inst, typeMap(refType->base()), Memory);
//            }
//            else {
//                value = Value(inst, Register);
//            }
//            /// Here we actually need to strip the reference because the
//            /// function may return a ref type
//            if (auto* arrayType =
//                    dyncast<sema::ArrayType const*>(stripRef(semaRetType)))
//            {
//                auto* size = ctx.intConstant(arrayType->size(), 64);
//                valueMap.insertArraySize(call.object(), Value(size, Register));
//            }
//        }
//        break;
//    }
//    case Memory: {
//        value = Value(irArguments.front(),
//                      typeMap(call.function()->returnType()),
//                      Memory);
//        auto* arrayType =
//            dyncast<sema::ArrayType const*>(call.function()->returnType());
//        if (arrayType) {
//            auto* size = ctx.intConstant(arrayType->size(), 64);
//            valueMap.insertArraySize(call.object(), Value(size, Register));
//        }
//        break;
//    }
//    }
//    return value;
//}

//void FuncGenContext::unpackArgument(PassingConvention const& PC,
//                                    sema::Type const* paramType,
//                                    ast::Expression const* expr,
//                                    Value value,
//                                    utl::vector<ir::Value*>& irArguments) {
//    if (!value) {
//        value = getValue(expr);
//    }
//    auto* object = expr->object();
//    if (isa<sema::ReferenceType>(paramType)) {
//        SC_ASSERT(value.isMemory(),
//                  "Need value in memory to pass by reference");
//        irArguments.push_back(toThinPointer(toMemory(value, *expr)));
//    }
//    else {
//        irArguments.push_back(
//            toThinPointer(toValueLocation(PC.location(), value, *expr)));
//    }
//    if (PC.numParams() == 2) {
//        irArguments.push_back(toRegister(valueMap.arraySize(object), *expr));
//    }
//}

utl::small_vector<ir::Value*> FuncGenContext::unpackArguments(
    std::span<PassingConvention const> PCs,
    std::span<sema::Type const* const> paramTypes,
    std::span<ast::Expression const* const> arguments,
    std::span<Value> argValues) {
    auto PCsAndArgs = ranges::views::zip(PCs, paramTypes, arguments, argValues);
    utl::small_vector<ir::Value*> irArguments;
    for (auto [PC, paramType, argExpr, argValue]: PCsAndArgs) {
        unpackArgument(PC, paramType, argExpr, argValue, irArguments);
    }
    return irArguments;
}

//Value FuncGenContext::getValueImpl(ast::Subscript const& expr) {
//    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
//    auto* array = toThinPointer(getValue<Memory>(expr.callee()));
//    /// Right now we don't use the size but here we could issue a call to an
//    /// assertion function
//    [[maybe_unused]] auto size = valueMap.arraySize(expr.callee()->object());
//    auto index = getValue<Register>(expr.arguments().front());
//    if (isFatPointer(arrayType->elementType())) {
//        auto* elemType = arrayPtrType;
//        auto* addr = add<ir::GetElementPointer>(elemType,
//                                                array,
//                                                index,
//                                                IndexArray{ 0 },
//                                                "elem");
//        Value result(addr, elemType->elementAt(0), Memory);
//        valueMap.insertArraySize(expr.object(), [=, this] {
//            auto* addr = add<ir::GetElementPointer>(elemType,
//                                                    array,
//                                                    index,
//                                                    IndexArray{ 1 },
//                                                    "elem.size");
//            return Value(addr, elemType->elementAt(1), Memory);
//        });
//        return result;
//    }
//    else {
//        auto* elemType = typeMap(arrayType->elementType());
//        auto* addr = add<ir::GetElementPointer>(elemType,
//                                                array,
//                                                index,
//                                                IndexArray{},
//                                                "elem.ptr");
//        return Value(addr, elemType, Memory);
//    }
//}

//Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
//    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
//    auto* elemType = typeMap(arrayType->elementType());
//    auto array = getValue(expr.callee());
//    auto lower = getValue<Register>(expr.lower());
//    auto upper = getValue<Register>(expr.upper());
//    SC_ASSERT(array.location() == Memory, "Must be in memory to be sliced");
//    auto* addr = add<ir::GetElementPointer>(elemType,
//                                            array.get(),
//                                            lower,
//                                            IndexArray{},
//                                            "elem.ptr");
//    Value result(addr, typeMap(expr.type()), Memory);
//    auto* size = add<ir::ArithmeticInst>(upper,
//                                         lower,
//                                         ir::ArithmeticOperation::Sub,
//                                         "slice.count");
//    valueMap.insertArraySize(expr.object(), Value(size, Register));
//    return result;
//}

//ir::ArrayType const* FuncGenContext::getListType(
//    ast::ListExpression const& list) {
//    auto* semaType = cast<sema::ArrayType const*>(list.type().get());
//    SC_ASSERT(!semaType->isDynamic(), "");
//    if (isFatPointer(semaType->elementType())) {
//        return ctx.arrayType(arrayPtrType, semaType->count());
//    }
//    return cast<ir::ArrayType const*>(typeMap(semaType));
//}

static ir::Constant* evalConstant(ir::Context& ctx,
                                  ast::Expression const* expr) {
    if (auto* val = dyncast<sema::IntValue const*>(expr->constantValue())) {
        return ctx.intConstant(val->value());
    }
    return nullptr;
}

//bool FuncGenContext::genStaticListData(ast::ListExpression const& list,
//                                       ir::Alloca* dest) {
//    auto* type = cast<sema::ArrayType const*>(list.type().get());
//    auto* elemType = type->elementType();
//    utl::small_vector<ir::Constant*> elems;
//    elems.reserve(type->size());
//    for (auto* expr: list.elements()) {
//        SC_ASSERT(elemType == expr->type().get(), "Invalid type");
//        auto* constant = evalConstant(ctx, expr);
//        if (!constant) {
//            return false;
//        }
//        elems.push_back(constant);
//    }
//    auto* irType = ctx.arrayType(typeMap(elemType), list.elements().size());
//    auto* value = ctx.arrayConstant(elems, irType);
//    auto name = nameFromSourceLoc("array", list.sourceLocation());
//    auto* source = mod.makeGlobalConstant(ctx, value, std::move(name));
//    callMemcpy(dest, source, irType->size());
//    return true;
//}

//void FuncGenContext::genDynamicListData(ast::ListExpression const& list,
//                                        ir::Alloca* dest) {
//    auto* arrayType = cast<sema::ArrayType const*>(list.type().get());
//    auto* elemType = getListType(list)->elementType();
//    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
//        if (isFatPointer(arrayType->elementType())) {
//            auto* ptr =
//                add<ir::GetElementPointer>(elemType,
//                                           dest,
//                                           ctx.intConstant(index, 32),
//                                           IndexArray{ 0 },
//                                           utl::strcat("elem.ptr.", index));
//            add<ir::Store>(ptr, getValue<Register>(elem));
//            auto* size =
//                add<ir::GetElementPointer>(elemType,
//                                           dest,
//                                           ctx.intConstant(index, 32),
//                                           IndexArray{ 1 },
//                                           utl::strcat("elem.size.", index));
//            add<ir::Store>(size,
//                           toRegister(valueMap.arraySize(elem->object()),
//                                      list));
//        }
//        else {
//            auto* gep = add<ir::GetElementPointer>(elemType,
//                                                   dest,
//                                                   ctx.intConstant(index, 32),
//                                                   IndexArray{},
//                                                   utl::strcat("elem.", index));
//            auto value = getValue(elem);
//
//            if (arrayType->elementType()->hasTrivialLifetime()) {
//                add<ir::Store>(gep, toRegister(value, list));
//            }
//            else {
//                toMemory(value, list)->replaceAllUsesWith(gep);
//            }
//        }
//    }
//}

//Value FuncGenContext::getValueImpl(ast::ListExpression const& list) {
//    auto* semaType = cast<sema::ArrayType const*>(list.type().get());
//    auto* irType = getListType(list);
//    auto* array = makeLocalVariable(irType, "list");
//    Value size(ctx.intConstant(list.children().size(), 64), Register);
//    /// We try to insert because a list expression of the same type might have
//    /// already added the value here
//    valueMap.tryInsert(semaType->findProperty(sema::PropertyKind::ArraySize),
//                       size);
//    auto value = Value(array, irType, Memory);
//    if (!genStaticListData(list, array)) {
//        genDynamicListData(list, array);
//    }
//    valueMap.insertArraySize(list.object(), size);
//    return value;
//}

//Value FuncGenContext::getValueImpl(ast::MoveExpr const& expr) {
//    auto value = getValue(expr.value());
//    auto* ctor = expr.function();
//    if (!ctor) {
//        valueMap.insertArraySizeOf(expr.object(), expr.value()->object());
//        return value;
//    }
//    auto* type = typeMap(expr.type());
//    auto* function = getFunction(ctor);
//    auto* dest =
//        makeLocalVariable(type, utl::strcat(value.get()->name(), ".moved"));
//    add<ir::Call>(function,
//                  ValueArray{ dest, toMemory(value, expr) },
//                  std::string{});
//    Value result(dest, type, Memory);
//    /// TODO: Handle case for dynamic array moved by value
//    if (isFatPointer(&expr)) {
//        auto* addr = add<ir::GetElementPointer>(ctx,
//                                                arrayPtrType,
//                                                dest,
//                                                nullptr,
//                                                IndexArray{ 1 },
//                                                "arraysize");
//        valueMap.insertArraySize(expr.object(),
//                                 Value(addr, ctx.intType(64), Memory));
//    }
//    return result;
//}

//Value FuncGenContext::getValueImpl(ast::UniqueExpr const& expr) {
//    auto backItr = std::prev(currentBlock().end());
//    auto* addr = getValue<Memory>(expr.value());
//    SC_ASSERT(
//        isa<ir::Alloca>(addr) || isa<ir::NullPointerConstant>(addr),
//        "We expect the argument to be constructed in local memory and we will rewrite it to heap allocation");
//    ir::Instruction* insertBefore = backItr->next();
//    ir::Callable* alloc = getBuiltin(svm::Builtin::alloc);
//    sema::ObjectType const* baseType =
//        cast<sema::UniquePtrType const&>(*expr.type()).base().get();
//    ir::Value* bytesize = [&]() -> ir::Value* {
//        if (auto* arrayType = dyncast<sema::ArrayType const*>(baseType)) {
//            size_t elemSize = arrayType->elementType()->size();
//            auto* count =
//                toRegister(valueMap.arraySize(expr.value()->object()), expr);
//            if (auto* countInst = dyncast<ir::Instruction*>(count)) {
//                insertBefore = countInst->next();
//            }
//            return withBlockCurrent(&currentBlock(),
//                                    ir::BasicBlock::Iterator(insertBefore),
//                                    [&] {
//                return makeCountToByteSize(count, elemSize);
//            });
//        }
//        else {
//            return ctx.intConstant(baseType->size(), 64);
//        }
//    }();
//    ValueArray args = { bytesize, ctx.intConstant(baseType->align(), 64) };
//    ir::Value* arrayPtr = insert<ir::Call>(insertBefore, alloc, args, "alloc");
//    ir::Value* ptr = insert<ir::ExtractValue>(insertBefore,
//                                              arrayPtr,
//                                              IndexArray{ 0 },
//                                              "pointer");
//    addr->replaceAllUsesWith(ptr);
//    if (!isFatPointer(&expr)) {
//        return Value(storeToMemory(ptr), ctx.ptrType(), Memory);
//    }
//    else {
//        auto* count =
//            toRegister(valueMap.arraySize(expr.value()->object()), expr);
//        arrayPtr = makeArrayPointer(ptr, count);
//        auto* ptrAddr = storeToMemory(arrayPtr);
//        valueMap.insertArraySize(expr.object(), [=, this] {
//            auto* sizeAddr =
//                add<ir::GetElementPointer>(ctx,
//                                           arrayPtrType,
//                                           ptrAddr,
//                                           nullptr,
//                                           IndexArray{ 1 },
//                                           "unique.array.size.addr");
//            return Value(sizeAddr, ctx.intType(64), Memory);
//        });
//        return Value(ptrAddr, arrayPtrType, Memory);
//    }
//}

static sema::ObjectType const* stripPtr(sema::ObjectType const* type) {
    if (auto* ptrType = dyncast<sema::PointerType const*>(type)) {
        return ptrType->base().get();
    }
    return type;
}

//Value FuncGenContext::getValueImpl(ast::Conversion const& conv) {
//    auto* expr = conv.expression();
//    Value refConvResult = [&]() -> Value {
//        switch (conv.conversion()->valueCatConversion()) {
//        case sema::ValueCatConversion::None:
//            [[fallthrough]];
//        case sema::ValueCatConversion::LValueToRValue:
//            return getValue(expr);
//
//        case sema::ValueCatConversion::MaterializeTemporary: {
//            auto value = getValue(expr);
//            return Value(toMemory(value, conv), value.type(), Memory);
//        }
//        }
//        SC_UNREACHABLE();
//    }();
//
//    switch (conv.conversion()->objectConversion()) {
//        using enum sema::ObjectTypeConversion;
//    case None:
//        return refConvResult;
//    case NullPtrToPtr:
//        if (isFatPointer(&conv)) {
//            valueMap.insertArraySize(conv.object(),
//                                     Value(ctx.intConstant(0, 64), Register));
//        }
//        return refConvResult;
//    case NullPtrToUniquePtr: {
//        Value value(toMemory(refConvResult, conv),
//                    refConvResult.type(),
//                    Memory);
//        if (isFatPointer(&conv)) {
//            valueMap.insertArraySize(conv.object(),
//                                     Value(ctx.intConstant(0, 64), Register));
//        }
//        return value;
//    }
//    case UniquePtrToPtr:
//        valueMap.insertArraySizeOf(conv.object(), conv.expression()->object());
//        return refConvResult;
//    case Array_FixedToDynamic: {
//        valueMap.insertArraySizeOf(conv.object(), expr->object());
//        if (!isa<sema::PointerType>(*expr->type())) {
//            return refConvResult;
//        }
//        auto* count =
//            toRegister(valueMap.arraySize(conv.expression()->object()), conv);
//        auto* arrayPtr =
//            makeArrayPointer(toRegister(refConvResult, conv), count);
//        return Value(arrayPtr, Register);
//    }
//    case Reinterpret_Array_ToByte:
//        [[fallthrough]];
//    case Reinterpret_Array_FromByte: {
//        auto* fromType =
//            cast<sema::ArrayType const*>(stripPtr(expr->type().get()));
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
//                    add<ir::ArithmeticInst>(toRegister(fromCount, conv),
//                                            ctx.intConstant(toElemSize, 64),
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
//                                         fromType->count() * fromElemSize);
//                break;
//            case Reinterpret_Array_FromByte:
//                valueMap.insertArraySize(conv.object(),
//                                         fromType->count() / toElemSize);
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
//            cast<sema::ArrayType const*>(stripPtr(expr->type().get()));
//        auto* toType = stripPtr(conv.type().get());
//        if (!fromType->isDynamic()) {
//            SC_ASSERT(fromType->size() == toType->size(), "Size mismatch");
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
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
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
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::Trunc,
//                                               "trunc");
//        return Value(result, Register);
//    }
//    case SS_Widen:
//        [[fallthrough]];
//    case SU_Widen: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::Sext,
//                                               "sext");
//        return Value(result, Register);
//    }
//    case US_Widen:
//        [[fallthrough]];
//    case UU_Widen: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::Zext,
//                                               "zext");
//        return Value(result, Register);
//    }
//    case Float_Trunc: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::Ftrunc,
//                                               "ftrunc");
//        return Value(result, Register);
//    }
//    case Float_Widen: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::Fext,
//                                               "fext");
//        return Value(result, Register);
//    }
//    case SignedToFloat: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::StoF,
//                                               "stof");
//        return Value(result, Register);
//    }
//    case UnsignedToFloat: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::UtoF,
//                                               "utof");
//        return Value(result, Register);
//    }
//    case FloatToSigned: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::FtoS,
//                                               "ftos");
//        return Value(result, Register);
//    }
//    case FloatToUnsigned: {
//        auto* result = add<ir::ConversionInst>(toRegister(refConvResult, conv),
//                                               typeMap(conv.type()),
//                                               ir::Conversion::FtoU,
//                                               "ftou");
//        return Value(result, Register);
//    }
//    }
//    SC_UNREACHABLE();
//}

//Value FuncGenContext::getValueImpl(ast::UninitTemporary const& temp) {
//    auto* type = [&]() -> ir::Type const* {
//        if (isFatPointer(&temp)) {
//            return arrayPtrType;
//        }
//        return typeMap(temp.type());
//    }();
//    auto* address = makeLocalVariable(type, "anon");
//    return Value(address, type, Memory);
//}

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

//Value FuncGenContext::genConstructDynArray(ast::ConstructExpr const& expr,
//                                           sema::ArrayType const& type) {
//    auto* elemType = typeMap(type.elementType());
//    switch (expr.arguments().size()) {
//    case 0:
//        /// No arguments mean we construct an array of size 0
//        valueMap.insertArraySize(expr.object(),
//                                 Value(ctx.intConstant(0, 64), Register));
//        return Value(ctx.nullpointer(), elemType, Memory);
//
//    case 1: {
//        auto* arg = expr.argument(0);
//        if (isa<sema::IntType>(*arg->type())) {
//            /// We construct a dynamic array from a count
//            auto* count = getValue<Register>(arg);
//            auto* storage = makeLocalArray(elemType, count, "dynarray");
//            if (expr.function()) {
//                add<ir::Call>(getFunction(expr.function()),
//                              ValueArray{ storage, count },
//                              std::string{});
//            }
//            else {
//                auto* numBytes = makeCountToByteSize(count, elemType->size());
//                callMemset(storage, numBytes, 0);
//            }
//            valueMap.insertArraySize(expr.object(), Value(count, Register));
//            return Value(storage, elemType, Memory);
//        }
//        else {
//            /// We construct a dynamic array from another one
//            SC_ASSERT(cast<sema::ArrayType const*>(arg->type().get())
//                              ->elementType() == type.elementType(),
//                      "Copy construction is the only other case");
//            auto argValue = getValue(arg);
//            SC_ASSERT(argValue.isMemory(), "Dynamic array must be in memory");
//            auto* count = toRegister(valueMap.arraySize(arg->object()), expr);
//            auto* storage = makeLocalArray(elemType, count, "dynarray");
//            if (auto* ctor = expr.function()) {
//                auto* irCtor = getFunction(ctor);
//                ValueArray irArguments = {
//                    storage,
//                    count,
//                    argValue.get(),
//                    count,
//                };
//                add<ir::Call>(irCtor, irArguments, std::string{});
//            }
//            else {
//                auto* numBytes = makeCountToByteSize(count, elemType->size());
//                callMemcpy(storage, toMemory(argValue, expr), numBytes);
//            }
//            valueMap.insertArraySize(expr.object(), Value(count, Register));
//            return Value(storage, elemType, Memory);
//        }
//    }
//    default:
//        SC_UNREACHABLE();
//    }
//}

//Value FuncGenContext::genConstructNonTrivial(ast::ConstructExpr const& expr) {
//    SC_EXPECT(!expr.isTrivial());
//    auto* type = typeMap(expr.constructedType());
//    auto* function = getFunction(expr.function());
//    auto CC = getCC(expr.function());
//    /// We generate all argument expressions before we unpack them because
//    /// we may need the array size of the second argument when unpacking the
//    /// first argument
//    auto argValues = expr.arguments() |
//                     transform([&](auto* arg) { return getValue(arg); }) |
//                     ToSmallVector<>;
//    /// If we invoke a copy constructor of a dynamic array type, we give the
//    /// first argument which is an uninit temporary the array size of the
//    /// second argument
//    if (isCopyCtor(*expr.function())) {
//        valueMap.insertArraySizeOf(expr.argument(0)->object(),
//                                   expr.argument(1)->object());
//    }
//    auto irArguments = unpackArguments(CC.arguments(),
//                                       expr.function()->argumentTypes(),
//                                       expr.arguments() | ToSmallVector<>,
//                                       argValues);
//    SC_ASSERT(!irArguments.empty(), "Must have at least the object argument");
//    add<ir::Call>(function, irArguments, std::string{});
//
//    SC_ASSERT(!getDynArrayType(expr.type().get()), "");
//
//    /// For unique pointers we lazily load the array size from memory
//    if (isFatPointer(&expr)) {
//        valueMap.insertArraySize(expr.object(), [=, this] {
//            auto* size = add<ir::GetElementPointer>(ctx,
//                                                    arrayPtrType,
//                                                    irArguments.front(),
//                                                    nullptr,
//                                                    IndexArray{ 1 },
//                                                    "arraysize");
//            return Value(size, ctx.intType(64), Memory);
//        });
//    }
//    return Value(irArguments.front(), type, Memory);
//}

Value FuncGenContext::genConstructTrivial(ast::ConstructExpr const& expr,
                                          sema::Type const& type) {
    return visit(type, [&](auto& type) SC_NODEBUG {
        return genConstructTrivialImpl(expr, type);
    });
}

Value FuncGenContext::genConstructTrivialImpl(ast::ConstructExpr const& expr,
                                              sema::ObjectType const& type) {
    if (expr.arguments().size() == 1) {
        return getValue(expr.arguments().front());
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
    return Value("tmp", expr.type().get(), {value}, Register, Packed);
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
    SC_UNIMPLEMENTED();
//    switch (expr.arguments().size()) {
//    case 0: {
//        auto* irType = typeMap(&type);
//        auto* addr = makeLocalVariable(irType, "tmp.array");
//        callMemset(addr, irType->size(), 0);
//        return Value(addr, irType, Memory);
//    }
//    case 1: {
//        auto* arg = expr.arguments().front();
//        if (type.isComplete() && type.size() <= 64) {
//            auto* value = getValue<Register>(arg);
//            return Value(value, Register);
//        }
//        else {
//            auto source = getValue(arg);
//            SC_ASSERT(source.isMemory(), "");
//            auto* elemType = typeMap(type.elementType());
//            auto* count =
//                toRegister(valueMap.arraySize(expr.argument(0)->object()),
//                           expr);
//            auto* array = makeLocalArray(elemType, count, "tmp.array");
//            auto* byteSize = makeCountToByteSize(count, elemType->size());
//            callMemcpy(array, source.get(), byteSize);
//            valueMap.insertArraySize(expr.object(), Value(count, Register));
//            return Value(array, elemType, Memory);
//        }
//    }
//    default:
//        SC_UNIMPLEMENTED();
//    }
}

//Value FuncGenContext::getValueImpl(ast::NonTrivAssignExpr const& expr) {
//    /// If the values are different, we  call the destructor of LHS and the copy
//    /// or move constructor of LHS with RHS as argument. If the values are the
//    /// same we do nothing
//    auto* dest = getValue<Memory>(expr.dest());
//    auto* source = getValue<Memory>(expr.source());
//    /// We branch here because the values might be the same.
//    auto* addrEq = add<ir::CompareInst>(dest,
//                                        source,
//                                        ir::CompareMode::Unsigned,
//                                        ir::CompareOperation::NotEqual,
//                                        "addr.eq");
//    auto* assignBlock = newBlock("assign");
//    auto* endBlock = newBlock("assign.end");
//    add<ir::Branch>(addrEq, assignBlock, endBlock);
//    add(assignBlock);
//    callDtor(expr.dest()->object(), expr.dtor(), expr);
//    auto* function = getFunction(expr.ctor());
//    add<ir::Call>(function, ValueArray{ dest, source }, std::string{});
//    add<ir::Goto>(endBlock);
//    add(endBlock);
//    return Value();
//}

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

//ir::Value* FuncGenContext::toRegister(Value value,
//                                      ast::ASTNode const& sourceNode) {
//    if (isa<ir::RecordType>(value.type())) {
//        auto* semaType = typeMap(value.type());
//        SC_ASSERT(!semaType || semaType->hasTrivialLifetime(),
//                  "We can only have trivial lifetime types in registers");
//    }
//    switch (value.location()) {
//    case Register:
//        return value.get();
//    case Memory:
//        auto* load = add<ir::Load>(value.get(),
//                                   value.type(),
//                                   std::string(value.get()->name()));
//        addSourceLoc(load, sourceNode);
//        return load;
//    }
//    SC_UNREACHABLE();
//}

//ir::Value* FuncGenContext::toMemory(Value value, ast::ASTNode const&) {
//    switch (value.location()) {
//    case Register:
//        return storeToMemory(value.get());
//
//    case Memory:
//        return value.get();
//    }
//    SC_UNREACHABLE();
//}

ir::Value* FuncGenContext::toValueLocation(ValueLocation location,
                                           Value value,
                                           ast::ASTNode const& sourceNode) {
    switch (location) {
    case Register:
        return toRegister(value, sourceNode);
    case Memory:
        return toMemory(value, sourceNode);
    }
    SC_UNREACHABLE();
}

void FuncGenContext::addSourceLoc(ir::Instruction* inst,
                                  ast::ASTNode const& sourceNode) {
    if (!config.generateDebugSymbols) {
        return;
    }
    inst->setMetadata(sourceNode.sourceRange().begin());
}
