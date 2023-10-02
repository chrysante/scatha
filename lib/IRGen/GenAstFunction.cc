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
using enum ValueLocation;

namespace {

struct Loop {
    ir::BasicBlock* header = nullptr;
    ir::BasicBlock* body = nullptr;
    ir::BasicBlock* inc = nullptr;
    ir::BasicBlock* end = nullptr;
};

struct FuncGenContext: FuncGenContextBase {
    /// Local state
    ValueMap valueMap;
    utl::stack<Loop, 4> loopStack;

    FuncGenContext(auto&... args): FuncGenContextBase(args...), valueMap(ctx) {}

    /// # Statements
    void generate(ast::Statement const&);

    void generateImpl(ast::Statement const&) { SC_UNREACHABLE(); }
    void generateImpl(ast::CompoundStatement const&);
    void generateImpl(ast::FunctionDefinition const&);
    void generateParameter(ast::ParameterDeclaration const* paramDecl,
                           PassingConvention pc,
                           List<ir::Parameter>::iterator& paramItr);
    void generateImpl(ast::VariableDeclaration const&);
    void generateImpl(ast::ExpressionStatement const&);
    void generateImpl(ast::EmptyStatement const&) {}
    void generateImpl(ast::ReturnStatement const&);
    void generateImpl(ast::IfStatement const&);
    void generateImpl(ast::LoopStatement const&);
    void generateImpl(ast::JumpStatement const&);

    /// # Statement specific utilities
    void callDtor(sema::Object const* object, sema::Function const* dtor);
    void emitDtorCalls(sema::DTorStack const& dtorStack);

    /// Creates array size values and stores them in `objectMap` if declared
    /// type is array
    void generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                  sema::Object const* initObject);

    /// # Expressions
    Value getValue(ast::Expression const* expr);

    template <ValueLocation Loc>
    ir::Value* getValue(ast::Expression const* expr);

    Value getValueImpl(ast::Expression const& expr) { SC_UNREACHABLE(); }
    Value getValueImpl(ast::Identifier const&);
    Value getValueImpl(ast::Literal const&);
    Value getValueImpl(ast::UnaryExpression const&);
    Value getValueImpl(ast::BinaryExpression const&);
    Value getValueImpl(ast::MemberAccess const&);
    Value getValueImpl(ast::DereferenceExpression const&);
    Value getValueImpl(ast::AddressOfExpression const&);
    Value getValueImpl(ast::Conditional const&);
    Value getValueImpl(ast::FunctionCall const&);
    Value getValueImpl(ast::Subscript const&);
    Value getValueImpl(ast::SubscriptSlice const&);
    Value getValueImpl(ast::ListExpression const&);
    Value getValueImpl(ast::Conversion const&);
    Value getValueImpl(ast::UninitTemporary const&);
    Value getValueImpl(ast::ConstructExpr const&);
    Value getValueImpl(ast::NonTrivAssignExpr const&);

    /// # Expression specific utilities
    void generateArgument(PassingConvention const& PC,
                          sema::Type const* paramType,
                          ast::Expression const* expr,
                          utl::vector<ir::Value*>& irArgsOut);
    bool genStaticListData(ast::ListExpression const& list, ir::Alloca* dest);
    void genListDataFallback(ast::ListExpression const& list, ir::Alloca* dest);

    /// # General utilities

    /// If the value \p value is already in a register, returns that.
    /// Otherwise loads the value from memory and returns the `load` instruction
    ir::Value* toRegister(Value value);

    /// If the value \p value is in memory, returns the address.
    /// Otherwise allocates stack memory, stores the value and returns the
    /// address
    ir::Value* toMemory(Value value);

    /// \Returns `toRegister(value)` or `toMemory(value)` depending on \p
    /// location
    ir::Value* toValueLocation(ValueLocation location, Value value);
};

} // namespace

utl::small_vector<sema::Function const*> irgen::generateAstFunction(
    ast::FunctionDefinition const& funcDecl,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap) {
    utl::small_vector<sema::Function const*> declaredFunctions;
    FuncGenContext funcCtx(*funcDecl.function(),
                           irFn,
                           ctx,
                           mod,
                           symbolTable,
                           typeMap,
                           functionMap,
                           declaredFunctions);
    funcCtx.generate(funcDecl);
    return declaredFunctions;
}

/// MARK: - Statements

void FuncGenContext::generate(ast::Statement const& node) {
    visit(node, [this](auto const& node) { return generateImpl(node); });
}

void FuncGenContext::generateImpl(ast::CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    emitDtorCalls(cmpStmt.dtorStack());
}

void FuncGenContext::generateImpl(ast::FunctionDefinition const& def) {
    addNewBlock("entry");
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
    finish();
}

void FuncGenContext::generateParameter(
    ast::ParameterDeclaration const* paramDecl,
    PassingConvention pc,
    List<ir::Parameter>::iterator& irParamItr) {
    sema::Type const* semaType = paramDecl->type();
    auto* irParam = irParamItr.to_address();
    auto* irType = typeMap(paramDecl->type());
    std::string name(paramDecl->name());
    auto* paramVar = paramDecl->variable();
    switch (pc.location()) {
    case Register: {
        bool const isDynArray = isPtrOrRefToDynArray(semaType);
        auto* refType = dyncast<sema::ReferenceType const*>(paramDecl->type());
        /// It's kind of annoying that we still need two cases here, but in one
        /// case we keep the pointer and the array size in a register and in the
        /// other case we store it to memory. Seems to complicated to merge the
        /// cases.
        if (refType) {
            valueMap.insert(paramVar,
                            Value(irParam, typeMap(refType->base()), Memory));
            ++irParamItr;
            if (isDynArray) {
                Value size(irParam->next(), Register);
                valueMap.insertArraySize(paramVar, size);
                ++irParamItr;
            }
        }
        else {
            valueMap.insert(paramVar,
                            Value(storeToMemory(irParam, name),
                                  irType,
                                  Memory));
            ++irParamItr;
            if (isDynArray) {
                Value size(storeToMemory(irParam->next()),
                           irParam->next()->type(),
                           Memory);
                valueMap.insertArraySize(paramVar, size);
                ++irParamItr;
            }
        }
        break;
    }

    case Memory: {
        Value data(irParam, irType, Memory);
        valueMap.insert(paramVar, data);
        ++irParamItr;
        break;
    }
    }
}

void FuncGenContext::generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                              sema::Object const* initObject) {
    if (!isPtrOrRefToDynArray(varDecl->type())) {
        return;
    }
    auto size = valueMap.arraySize(initObject);
    if (isa<sema::ReferenceType>(varDecl->type())) {
        valueMap.insertArraySize(varDecl->variable(),
                                 Value(toRegister(size), Register));
    }
    else {
        auto* newSize = storeToMemory(toRegister(size),
                                      utl::strcat(varDecl->name(), ".size"));
        valueMap.insertArraySize(varDecl->variable(),
                                 Value(newSize, size.type(), Memory));
    }
}

void FuncGenContext::generateImpl(ast::VariableDeclaration const& varDecl) {
    auto dtorStack = varDecl.dtorStack();
    std::string name = std::string(varDecl.name());
    auto* initExpr = varDecl.initExpr();
    if (isa<sema::ReferenceType>(varDecl.type())) {
        SC_ASSERT(initExpr, "Reference must be initialized");
        auto value = getValue(initExpr);
        valueMap.insert(varDecl.variable(), value);
        generateVarDeclArraySize(&varDecl, initExpr->object());
    }
    else if (initExpr) {
        /// If we have non-trivial types sema ensures that we always have an
        /// init-expr in memory
        auto value = getValue(initExpr);
        auto* address = toMemory(value);
        address->setName(name);
        valueMap.insert(varDecl.variable(),
                        Value(address, value.type(), Memory));
        generateVarDeclArraySize(&varDecl, initExpr->object());
    }
    else {
        /// Here we could initialize the memory
        auto* type = typeMap(varDecl.type());
        auto* address = makeLocalVariable(type, name);
        valueMap.insert(varDecl.variable(), Value(address, type, Memory));
        generateVarDeclArraySize(&varDecl, nullptr);
    }
    emitDtorCalls(dtorStack);
}

void FuncGenContext::generateImpl(
    ast::ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    emitDtorCalls(exprStatement.dtorStack());
}

void FuncGenContext::generateImpl(ast::ReturnStatement const& retStmt) {
    if (!retStmt.expression()) {
        emitDtorCalls(retStmt.dtorStack());
        add<ir::Return>(ctx.voidValue());
        return;
    }
    auto retval = getValue(retStmt.expression());
    ir::Value* actualRetval = nullptr;
    auto retvalLocation = getCC(&semaFn).returnValue().location();
    switch (retvalLocation) {
    case Register: {
        /// Pointers we keep in registers but references directly refer to the
        /// value in memory
        auto valueLocation =
            isa<sema::ReferenceType>(*semaFn.returnType()) ? Memory : Register;
        if (isPtrOrRefToDynArray(semaFn.returnType())) {
            auto size = valueMap.arraySize(retStmt.expression()->object());
            std::array elems = { toValueLocation(valueLocation, retval),
                                 toRegister(size) };
            actualRetval =
                buildStructure(makeArrayViewType(ctx), elems, "retval");
        }
        else {
            actualRetval = toValueLocation(valueLocation, retval);
        }
        break;
    }
    case Memory: {
        auto* retvalDest = &irFn.parameters().front();
        if (retval.isMemory()) {
            if (auto* allocaInst = dyncast<ir::Alloca*>(retval.get())) {
                allocaInst->replaceAllUsesWith(retvalDest);
            }
            else {
                callMemcpy(retvalDest, toMemory(retval), retval.type()->size());
            }
        }
        else {
            add<ir::Store>(retvalDest, toRegister(retval));
        }
        actualRetval = ctx.voidValue();
        break;
    }
    }
    /// We call destructors as the very last step before issuing the return
    /// instruction
    emitDtorCalls(retStmt.dtorStack());
    add<ir::Return>(actualRetval);
}

void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
    auto* condition = getValue<Register>(stmt.condition());
    emitDtorCalls(stmt.dtorStack());
    auto* thenBlock = newBlock("if.then");
    auto* elseBlock = stmt.elseBlock() ? newBlock("if.else") : nullptr;
    auto* endBlock = newBlock("if.end");
    add<ir::Branch>(condition, thenBlock, elseBlock ? elseBlock : endBlock);
    add(thenBlock);
    generate(*stmt.thenBlock());
    add<ir::Goto>(endBlock);
    if (stmt.elseBlock()) {
        add(elseBlock);
        generate(*stmt.elseBlock());
        add<ir::Goto>(endBlock);
    }
    add(endBlock);
}

void FuncGenContext::generateImpl(ast::LoopStatement const& loopStmt) {
    switch (loopStmt.kind()) {
    case ast::LoopKind::For: {
        auto* loopHeader = newBlock("loop.header");
        auto* loopBody = newBlock("loop.body");
        auto* loopInc = newBlock("loop.inc");
        auto* loopEnd = newBlock("loop.end");
        generate(*loopStmt.varDecl());
        add<ir::Goto>(loopHeader);

        /// Header
        add(loopHeader);
        auto* condition = getValue<Register>(loopStmt.condition());
        emitDtorCalls(loopStmt.conditionDtorStack());
        add<ir::Branch>(condition, loopBody, loopEnd);
        loopStack.push({ .header = loopHeader,
                         .body = loopBody,
                         .inc = loopInc,
                         .end = loopEnd });

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopInc);

        /// Inc
        add(loopInc);
        getValue(loopStmt.increment());
        emitDtorCalls(loopStmt.incrementDtorStack());
        add<ir::Goto>(loopHeader);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }

    case ast::LoopKind::While: {
        auto* loopHeader = newBlock("loop.header");
        auto* loopBody = newBlock("loop.body");
        auto* loopEnd = newBlock("loop.end");
        add<ir::Goto>(loopHeader);

        /// Header
        add(loopHeader);
        auto* condition = getValue<Register>(loopStmt.condition());
        emitDtorCalls(loopStmt.conditionDtorStack());
        add<ir::Branch>(condition, loopBody, loopEnd);
        loopStack.push(
            { .header = loopHeader, .body = loopBody, .end = loopEnd });

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopHeader);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }

    case ast::LoopKind::DoWhile: {
        auto* loopBody = newBlock("loop.body");
        auto* loopFooter = newBlock("loop.footer");
        auto* loopEnd = newBlock("loop.end");
        add<ir::Goto>(loopBody);
        loopStack.push(
            { .header = loopFooter, .body = loopBody, .end = loopEnd });

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopFooter);

        /// Footer
        add(loopFooter);
        auto* condition = getValue<Register>(loopStmt.condition());
        emitDtorCalls(loopStmt.conditionDtorStack());
        add<ir::Branch>(condition, loopBody, loopEnd);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }
    }
    emitDtorCalls(loopStmt.dtorStack());
}

void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
    emitDtorCalls(jump.dtorStack());
    auto* dest = [&] {
        auto& currentLoop = loopStack.top();
        switch (jump.kind()) {
        case ast::JumpStatement::Break:
            return currentLoop.end;
        case ast::JumpStatement::Continue:
            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
        }
    }();
    add<ir::Goto>(dest);
}

/// MARK: - Expressions

static bool isIntType(size_t width, ir::Type const* type) {
    return cast<ir::IntegralType const*>(type)->bitwidth() == 1;
}

Value FuncGenContext::getValue(ast::Expression const* expr) {
    SC_ASSERT(expr, "");
    /// Returning constants here if possible breaks when we take the address of
    /// a constant. A solution that also solves the array size problem could be
    /// to add additional optional data to values (other values) that could get
    /// resovled by the `toRegister` function. I.e. when we call `getValue` on
    /// an identifier, we get a value that represents the value in memory, but
    /// is annotated with the constant. Then when we call `toRegister` on the
    /// value it checks whether the value is annotated with a constant and if so
    /// returns that. Otherwise it defaults to loading the value.
#if 0
    if (auto* constVal = expr->constantValue();
        constVal && !expr->type()->isReference())
    {
        auto* type = cast<sema::ArithmeticType
        const*>(expr->type()->base());
        // clang-format off
        return visit(*constVal, utl::overload{
            [&](sema::IntValue const& intVal) {
                SC_ASSERT(type->bitwidth() == intVal.value().bitwidth(),
                          "");
                return Value(intConstant(intVal.value()), Register);
            },
            [&](sema::FloatValue const& floatVal) {
                return Value(floatConstant(floatVal.value()), Register);
            }
        }); // clang-format on
    }
#endif
    return visit(*expr,
                 [this](auto const& expr) { return getValueImpl(expr); });
}

template <ValueLocation Loc>
ir::Value* FuncGenContext::getValue(ast::Expression const* expr) {
    return toValueLocation(Loc, getValue(expr));
}

Value FuncGenContext::getValueImpl(ast::Identifier const& id) {
    /// Because identifier expressions always have reference type, we take the
    /// address of the referred to value and put it in a register
    Value value = valueMap(id.object());
    return value; // Value(value.get(), Register);
}

Value FuncGenContext::getValueImpl(ast::Literal const& lit) {
    using enum ast::LiteralKind;
    switch (lit.kind()) {
    case Integer:
        return Value(ctx.intConstant(lit.value<APInt>()), Register);
    case Boolean:
        return Value(ctx.intConstant(lit.value<APInt>()), Register);
    case FloatingPoint:
        return Value(ctx.floatConstant(lit.value<APFloat>()), Register);
    case This:
        return valueMap(lit.object());
    case String: {
        std::string const& sourceText = lit.value<std::string>();
        size_t size = sourceText.size();
        utl::vector<u8> text(sourceText.begin(), sourceText.end());
        auto* type = ctx.arrayType(ctx.intType(8), size);
        auto staticData =
            allocate<ir::ConstantData>(ctx, type, std::move(text), "stringlit");
        auto data = Value(staticData.get(), staticData.get()->type(), Memory);
        mod.addConstantData(std::move(staticData));
        valueMap.insertArraySize(lit.object(), size);
        return data;
    }
    case Char:
        return Value(ctx.intConstant(lit.value<APInt>()), Register);
    }
}

Value FuncGenContext::getValueImpl(ast::UnaryExpression const& expr) {
    using enum ast::UnaryOperator;
    switch (expr.operation()) {
    case Increment:
        [[fallthrough]];
    case Decrement: {
        Value operand = getValue(expr.operand());
        SC_ASSERT(operand.isMemory(),
                  "Operand must be in memory to be modified");
        ir::Value* opAddr = toMemory(operand);
        ir::Type const* operandType = typeMap(expr.operand()->type());
        ir::Value* operandValue = toRegister(operand);
        auto* newValue =
            add<ir::ArithmeticInst>(operandValue,
                                    ctx.arithmeticConstant(1, operandType),
                                    expr.operation() == Increment ?
                                        ir::ArithmeticOperation::Add :
                                        ir::ArithmeticOperation::Sub,
                                    utl::strcat(expr.operation(), ".res"));
        add<ir::Store>(opAddr, newValue);
        switch (expr.notation()) {
        case ast::UnaryOperatorNotation::Prefix:
            return operand;
        case ast::UnaryOperatorNotation::Postfix:
            return Value(operandValue, Register);
        }
    }

    case ast::UnaryOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryOperator::Negation: {
        auto* operand = toRegister(getValue(expr.operand()));
        auto operation = isa<sema::IntType>(expr.operand()->type().get()) ?
                             ir::ArithmeticOperation::Sub :
                             ir::ArithmeticOperation::FSub;
        auto* newValue =
            add<ir::ArithmeticInst>(ctx.arithmeticConstant(0, operand->type()),
                                    operand,
                                    operation,
                                    "negated");
        return Value(newValue, Register);
    }

    default:
        auto* operand = toRegister(getValue(expr.operand()));
        auto* newValue =
            add<ir::UnaryArithmeticInst>(operand,
                                         mapUnaryOp(expr.operation()),
                                         "expr");
        return Value(newValue, Register);
    }
}

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
}

Value FuncGenContext::getValueImpl(ast::BinaryExpression const& expr) {
    auto* builtinType =
        dyncast<sema::BuiltinType const*>(expr.lhs()->type().get());
    auto resName = getResultName(expr.operation());
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
        auto* lhs = getValue<Register>(expr.lhs());
        auto* rhs = getValue<Register>(expr.rhs());
        auto operation = mapArithmeticOp(builtinType, expr.operation());
        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, resName);
        return Value(result, Register);
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue<Register>(expr.lhs());
        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
        auto* startBlock = &currentBlock();
        auto* rhsBlock = newBlock("log.rhs");
        auto* endBlock = newBlock("log.end");
        if (expr.operation() == LogicalAnd) {
            add<ir::Branch>(lhs, rhsBlock, endBlock);
        }
        else {
            add<ir::Branch>(lhs, endBlock, rhsBlock);
        }

        add(rhsBlock);
        auto* rhs = getValue<Register>(expr.rhs());
        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
        add<ir::Goto>(endBlock);
        add(endBlock);

        if (expr.operation() == LogicalAnd) {
            auto* result = add<ir::Phi>(
                std::array<ir::PhiMapping, 2>{
                    ir::PhiMapping{ startBlock, ctx.boolConstant(0) },
                    ir::PhiMapping{ rhsBlock, rhs } },
                "log.and");
            return Value(result, Register);
        }
        else {
            auto* result = add<ir::Phi>(
                std::array<ir::PhiMapping, 2>{
                    ir::PhiMapping{ startBlock, ctx.boolConstant(1) },
                    ir::PhiMapping{ rhsBlock, rhs } },
                "log.or");
            return Value(result, Register);
        }
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
        auto* result = add<ir::CompareInst>(getValue<Register>(expr.lhs()),
                                            getValue<Register>(expr.rhs()),
                                            mapCompareMode(builtinType),
                                            mapCompareOp(expr.operation()),
                                            resName);
        return Value(result, Register);
    }

    case Comma:
        getValue(expr.lhs());
        return getValue(expr.rhs());

    case Assignment: {
        auto lhs = getValue<Memory>(expr.lhs());
        auto rhs = getValue<Register>(expr.rhs());
        add<ir::Store>(lhs, rhs);
        /// It is never a reference because expressions don't have reference
        /// type
        if (isPtrOrRefToDynArray(expr.lhs()->type().get())) {
            auto lhsSize = valueMap.arraySize(expr.lhs()->object());
            SC_ASSERT(lhsSize.location() == Memory,
                      "Must be in memory to reassign");
            auto rhsSize = valueMap.arraySize(expr.rhs()->object());
            add<ir::Store>(lhsSize.get(), toRegister(rhsSize));
        }
        return Value();
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
        auto lhs = getValue(expr.lhs());
        SC_ASSERT(lhs.isMemory(), "");
        auto rhs = getValue<Register>(expr.rhs());
        SC_ASSERT(builtinType == expr.rhs()->type().get(), "");
        auto operation = mapArithmeticAssignOp(builtinType, expr.operation());
        rhs = add<ir::ArithmeticInst>(toRegister(lhs), rhs, operation, resName);
        add<ir::Store>(lhs.get(), rhs);
        return Value();
    }
    }
}

Value FuncGenContext::getValueImpl(ast::MemberAccess const& expr) {
    if (auto value = valueMap.tryGet(expr.member()->object())) {
        return *value;
    }
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(expr.accessed()->type().get()))
    {
        SC_ASSERT(expr.member()->value() == "count", "What else?");
        if (arrayType->isDynamic()) {
            getValue(expr.accessed());
            return valueMap.arraySize(expr.accessed()->object());
        }
        else {
            return Value(ctx.intConstant(arrayType->count(), 64), Register);
        }
    }

    Value base = getValue(expr.accessed());
    auto* var = cast<sema::Variable const*>(expr.member()->entity());

    Value value;

    auto const& metaData = typeMap.metaData(expr.accessed()->type().get());
    size_t const irIndex = metaData.indexMap[var->index()];
    switch (base.location()) {
    case Register: {
        auto* result =
            add<ir::ExtractValue>(base.get(), std::array{ irIndex }, "mem.acc");
        value = Value(result, Register);
        break;
    }
    case Memory: {
        auto* baseType = typeMap(expr.accessed()->type());
        auto* result = add<ir::GetElementPointer>(baseType,
                                                  base.get(),
                                                  ctx.intConstant(0, 64),
                                                  std::array{ irIndex },
                                                  "mem.acc");
        auto* accessedType = typeMap(var->type());
        value = Value(result, accessedType, Memory);
        break;
    }
    }
    sema::QualType memType = expr.type();
    if (isPtrOrRefToDynArray(memType.get())) {
        auto lazySize = [this, base, sizeIndex = irIndex + 1] {
            switch (base.location()) {
            case Register: {
                auto* result = add<ir::ExtractValue>(base.get(),
                                                     std::array{ sizeIndex },
                                                     "mem.acc.size");
                return Value(result, Register);
            }
            case Memory: {
                auto* result =
                    add<ir::GetElementPointer>(base.type(),
                                               base.get(),
                                               ctx.intConstant(0, 64),
                                               std::array{ sizeIndex },
                                               "mem.acc.size");
                return Value(result, ctx.intType(64), Memory);
            }
            }
        };
        valueMap.insertArraySize(expr.object(), lazySize);
    }
    return value;
}

Value FuncGenContext::getValueImpl(ast::DereferenceExpression const& expr) {
    auto* ptr = getValue<Register>(expr.referred());
    valueMap.insertArraySizeOf(expr.object(), expr.referred()->object());
    return Value(ptr, typeMap(expr.type()), Memory);
}

Value FuncGenContext::getValueImpl(ast::AddressOfExpression const& expr) {
    auto* ptr = getValue<Memory>(expr.referred());
    valueMap.insertArraySizeOf(expr.object(), expr.referred()->object());
    return Value(ptr, Register);
}

Value FuncGenContext::getValueImpl(ast::Conditional const& condExpr) {
    auto* cond = getValue<Register>(condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto thenVal = getValue(condExpr.thenExpr());
    thenBlock = &currentBlock(); /// Nested `?:` operands etc. may have changed
                                 /// `currentBlock`
    add<ir::Goto>(endBlock);

    /// Generate else block.
    add(elseBlock);
    auto elseVal = getValue(condExpr.elseExpr());
    elseBlock = &currentBlock();
    add<ir::Goto>(endBlock);

    /// Generate end block.
    add(endBlock);
    std::array phiArgs = { ir::PhiMapping{ thenBlock, thenVal.get() },
                           ir::PhiMapping{ elseBlock, elseVal.get() } };
    auto* result = add<ir::Phi>(phiArgs, "cond");
    SC_ASSERT(thenVal.location() == elseVal.location(),
              "Must be the same if we phi them here");
    return Value(result, typeMap(condExpr.type()), thenVal.location());
}

static sema::Type const* stripRef(sema::Type const* type) {
    if (auto* ref = dyncast<sema::ReferenceType const*>(type)) {
        return ref->base().get();
    }
    return type;
}

Value FuncGenContext::getValueImpl(ast::FunctionCall const& call) {
    ir::Callable* function = getFunction(call.function());
    auto CC = getCC(call.function());
    auto const retvalLocation = CC.returnValue().location();
    utl::small_vector<ir::Value*> irArguments;
    if (retvalLocation == Memory) {
        auto* returnType = typeMap(call.function()->returnType());
        irArguments.push_back(makeLocalVariable(returnType, "retval"));
    }
    for (auto [PC, paramType, argExpr]:
         ranges::views::zip(CC.arguments(),
                            call.function()->argumentTypes(),
                            call.arguments()))
    {
        generateArgument(PC, paramType, argExpr, irArguments);
    }
    bool callHasName = !isa<ir::VoidType>(function->returnType());
    std::string name = callHasName ? "call.result" : std::string{};
    auto* inst = add<ir::Call>(function, irArguments, std::move(name));
    auto semaRetType = call.function()->returnType();
    Value value;
    switch (retvalLocation) {
    case Register: {
        auto* refType = dyncast<sema::ReferenceType const*>(semaRetType);
        if (isPtrOrRefToDynArray(semaRetType)) {
            auto* data =
                add<ir::ExtractValue>(inst, std::array{ size_t{ 0 } }, "data");
            auto* size =
                add<ir::ExtractValue>(inst, std::array{ size_t{ 1 } }, "size");
            if (refType) {
                value = Value(data, typeMap(refType->base()), Memory);
            }
            else {
                value = Value(data, Register);
            }
            valueMap.insertArraySize(call.object(), Value(size, Register));
        }
        else {
            if (refType) {
                value = Value(inst, typeMap(refType->base()), Memory);
            }
            else {
                value = Value(inst, Register);
            }
            /// Here we actually need to strip the reference because the
            /// function may return a ref type
            if (auto* arrayType =
                    dyncast<sema::ArrayType const*>(stripRef(semaRetType)))
            {
                auto* size = ctx.intConstant(arrayType->size(), 64);
                valueMap.insertArraySize(call.object(), Value(size, Register));
            }
        }
        break;
    }
    case Memory: {
        value = Value(irArguments.front(),
                      typeMap(call.function()->returnType()),
                      Memory);
        auto* arrayType =
            dyncast<sema::ArrayType const*>(call.function()->returnType());
        if (arrayType) {
            auto* size = ctx.intConstant(arrayType->size(), 64);
            valueMap.insertArraySize(call.object(), Value(size, Register));
        }
        break;
    }
    }
    valueMap.insert(call.object(), value);
    return value;
}

void FuncGenContext::generateArgument(PassingConvention const& PC,
                                      sema::Type const* paramType,
                                      ast::Expression const* expr,
                                      utl::vector<ir::Value*>& irArguments) {
    auto value = getValue(expr);
    auto* object = expr->object();
    if (isa<sema::ReferenceType>(paramType)) {
        SC_ASSERT(value.isMemory(),
                  "Need value in memory to pass by reference");
        irArguments.push_back(toMemory(value));
    }
    else {
        irArguments.push_back(toValueLocation(PC.location(), value));
    }
    if (PC.numParams() == 2) {
        irArguments.push_back(toRegister(valueMap.arraySize(object)));
    }
}

Value FuncGenContext::getValueImpl(ast::Subscript const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
    auto* elemType = typeMap(arrayType->elementType());
    auto array = getValue(expr.callee());
    /// Right now we don't use the size but here we could issue a call to an
    /// assertion function
    [[maybe_unused]] auto size = valueMap.arraySize(expr.callee()->object());
    auto index = getValue<Register>(expr.arguments().front());
    auto* addr = add<ir::GetElementPointer>(elemType,
                                            toMemory(array),
                                            index,
                                            std::initializer_list<size_t>{},
                                            "elem.ptr");
    return Value(addr, elemType, Memory);
}

Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
    auto* elemType = typeMap(arrayType->elementType());
    auto array = getValue(expr.callee());
    auto lower = getValue<Register>(expr.lower());
    auto upper = getValue<Register>(expr.upper());
    SC_ASSERT(array.location() == Memory, "Must be in memory to be sliced");
    auto* addr = add<ir::GetElementPointer>(elemType,
                                            array.get(),
                                            lower,
                                            std::initializer_list<size_t>{},
                                            "elem.ptr");
    Value result(addr, typeMap(expr.type()), Memory);
    auto* size = add<ir::ArithmeticInst>(upper,
                                         lower,
                                         ir::ArithmeticOperation::Sub,
                                         "slice.count");
    valueMap.insertArraySize(expr.object(), Value(size, Register));
    return result;
}

static bool evalConstant(ast::Expression const* expr, utl::vector<u8>& dest) {
    auto* val = dyncast_or_null<sema::IntValue const*>(expr->constantValue());
    if (!val) {
        return false;
    }
    auto value = val->value();
    size_t const elemSize = expr->type()->size();
    auto* data = reinterpret_cast<u8 const*>(value.limbs().data());
    for (auto* end = data + elemSize; data < end; ++data) {
        dest.push_back(*data);
    }
    return true;
}

bool FuncGenContext::genStaticListData(ast::ListExpression const& list,
                                       ir::Alloca* dest) {
    auto* type = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = type->elementType();
    utl::small_vector<u8> data;
    data.reserve(type->size());
    for (auto* expr: list.elements()) {
        SC_ASSERT(elemType == expr->type().get(), "Invalid type");
        if (!evalConstant(expr, data)) {
            return false;
        }
    }
    auto* irType = ctx.arrayType(typeMap(elemType), list.elements().size());
    auto [index, line, column] = list.sourceLocation();
    auto name = utl::strcat("array.at[", line, ":", column, "]");
    auto constData =
        allocate<ir::ConstantData>(ctx, irType, std::move(data), name);
    auto* source = constData.get();
    mod.addConstantData(std::move(constData));
    callMemcpy(dest, source, list.elements().size() * elemType->size());
    return true;
}

void FuncGenContext::genListDataFallback(ast::ListExpression const& list,
                                         ir::Alloca* dest) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type().get());
    auto* elemType = typeMap(arrayType->elementType());
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* gep = add<ir::GetElementPointer>(elemType,
                                               dest,
                                               ctx.intConstant(index, 32),
                                               std::initializer_list<size_t>{},
                                               "elem.ptr");
        add<ir::Store>(gep, getValue<Register>(elem));
    }
}

Value FuncGenContext::getValueImpl(ast::ListExpression const& list) {
    auto* semaType = cast<sema::ArrayType const*>(list.type().get());
    auto* irType = typeMap(semaType);
    auto* array = makeLocalVariable(irType, "list");
    Value size(ctx.intConstant(list.children().size(), 64), Register);
    /// We try to insert because a list expression of the same type might have
    /// already added the value here
    valueMap.tryInsert(semaType->countProperty(), size);
    auto value = Value(array, irType, Memory);
    if (!genStaticListData(list, array)) {
        genListDataFallback(list, array);
    }
    valueMap.insertArraySize(list.object(), size);
    return value;
}

static sema::ArrayType const* toArrayStripPtr(sema::Type const* type) {
    if (auto* ptrType = dyncast<sema::PointerType const*>(type)) {
        type = ptrType->base().get();
    }
    return cast<sema::ArrayType const*>(type);
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
            return Value(toMemory(value), value.type(), Memory);
        }
        }
    }();

    switch (conv.conversion()->objectConversion()) {
        using enum sema::ObjectTypeConversion;
    case None:
        return refConvResult;

    case Array_FixedToDynamic: {
        valueMap.insertArraySize(conv.object(),
                                 valueMap.arraySize(expr->object()));
        return refConvResult;
    }
    case Reinterpret_Array_ToByte:
        [[fallthrough]];
    case Reinterpret_Array_FromByte: {
        auto* fromType = toArrayStripPtr(expr->type().get());
        auto* toType = toArrayStripPtr(conv.type().get());
        size_t fromCount = fromType->elementType()->size();
        size_t toCount = toType->elementType()->size();
        auto data = refConvResult;
        if (toType->isDynamic()) {
            if (fromType->isDynamic()) {
                auto count = valueMap.arraySize(expr->object());
                if (conv.conversion()->objectConversion() ==
                    Reinterpret_Array_ToByte)
                {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                ctx.intConstant(fromCount, 64),
                                                ir::ArithmeticOperation::Mul,
                                                "reinterpret.count");
                    count = Value(newCount, Register);
                }
                else {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                ctx.intConstant(toCount, 64),
                                                ir::ArithmeticOperation::SDiv,
                                                "reinterpret.count");
                    count = Value(newCount, Register);
                }
                valueMap.insertArraySize(conv.object(), count);
            }
            else {
                size_t count = fromType->count();
                switch (conv.conversion()->objectConversion()) {
                case Reinterpret_Array_ToByte:
                    count *= fromType->elementType()->size();
                    break;
                case Reinterpret_Array_FromByte:
                    count /= toType->elementType()->size();
                    break;
                default:
                    SC_UNREACHABLE();
                }
                valueMap.insertArraySize(conv.object(), count);
            }
        }
        else {
            SC_ASSERT(!fromType->isDynamic(), "Invalid conversion");
        }
        return data;
    }

    case Reinterpret_Value: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Bitcast,
                                               "reinterpret");
        return Value(result, Register);
    }
    case SS_Trunc:
        [[fallthrough]];
    case SU_Trunc:
        [[fallthrough]];
    case US_Trunc:
        [[fallthrough]];
    case UU_Trunc: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Trunc,
                                               "trunc");
        return Value(result, Register);
    }
    case SS_Widen:
        [[fallthrough]];
    case SU_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Sext,
                                               "sext");
        return Value(result, Register);
    }
    case US_Widen:
        [[fallthrough]];
    case UU_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Zext,
                                               "zext");
        return Value(result, Register);
    }
    case Float_Trunc: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Ftrunc,
                                               "ftrunc");
        return Value(result, Register);
    }
    case Float_Widen: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::Fext,
                                               "fext");
        return Value(result, Register);
    }
    case SignedToFloat: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::StoF,
                                               "stof");
        return Value(result, Register);
    }
    case UnsignedToFloat: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::UtoF,
                                               "utof");
        return Value(result, Register);
    }
    case FloatToSigned: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::FtoS,
                                               "ftos");
        return Value(result, Register);
    }
    case FloatToUnsigned: {
        auto* result = add<ir::ConversionInst>(toRegister(refConvResult),
                                               typeMap(conv.type()),
                                               ir::Conversion::FtoU,
                                               "ftou");
        return Value(result, Register);
    }
    }
}

Value FuncGenContext::getValueImpl(ast::UninitTemporary const& temp) {
    auto* type = typeMap(temp.type());
    auto* address = makeLocalVariable(type, "anon");
    return Value(address, type, Memory);
}

Value FuncGenContext::getValueImpl(ast::ConstructExpr const& expr) {
    /// If we have a constructor we just call that
    if (!expr.isTrivial()) {
        auto* type = typeMap(expr.constructedType());
        auto* function = getFunction(expr.function());
        auto CC = getCC(expr.function());
        utl::small_vector<ir::Value*> irArguments;
        auto PCsAndArgs = ranges::views::zip(CC.arguments(),
                                             expr.function()->argumentTypes(),
                                             expr.arguments());
        for (auto [PC, paramType, argExpr]: PCsAndArgs) {
            generateArgument(PC, paramType, argExpr, irArguments);
        }
        SC_ASSERT(!irArguments.empty(),
                  "Must have at least the object argument");
        auto* address = irArguments.front();
        valueMap.insert(expr.object(), Value(address, type, Memory));
        add<ir::Call>(function, irArguments, std::string{});
        return Value(address, type, Memory);
    }
    // clang-format off
    return SC_MATCH (*expr.type()){
        [&](sema::ObjectType const& type) {
            if (expr.arguments().empty()) {
                auto* value = ctx.undef(typeMap(expr.type()));
                return Value(value, Register);
            }
            else {
                SC_ASSERT(expr.arguments().size() == 1, "");
                auto* arg = expr.arguments().front();
                auto value = getValue(arg);
                valueMap.insertArraySizeOf(expr.object(), arg->object());
                return Value(toRegister(value), Register);
            }
        },
        [&](sema::StructType const& type) {
            if (expr.arguments().empty()) {
                auto* value = ctx.undef(typeMap(expr.type()));
                return Value(value, Register);
            }
            else if (expr.arguments().size() == 1 &&
                     expr.arguments().front()->type().get() == expr.type().get())
            {
                auto* arg = expr.arguments().front();
                auto value = getValue(arg);
                valueMap.insertArraySizeOf(expr.object(), arg->object());
                return Value(toRegister(value), Register);
            }
            else {
                ir::Value* aggregate = ctx.undef(typeMap(expr.type()));
                for (auto [index, arg]: expr.arguments() |
                                        ranges::views::enumerate)
                {
                    auto* member = getValue<Register>(arg);
                    aggregate = add<ir::InsertValue>(aggregate,
                                                     member,
                                                     std::array{ index },
                                                     "aggregate");
                }
                return Value(aggregate, Register);
            }
        },
        [&](sema::ArrayType const& type) -> Value {
            SC_ASSERT(expr.arguments().size() == 1, "");
            auto* arg = expr.arguments().front();
            if (type.size() <= 64) {
                auto value = getValue(arg);
                return Value(toRegister(value), Register);
            }
            else {
                auto source = getValue(arg);
                SC_ASSERT(source.isMemory(), "");
                auto* arrayType = typeMap(&type);
                auto* array = makeLocalVariable(arrayType, "list");
                callMemcpy(array, source.get(), type.size());
                return Value(array, arrayType, Memory);
            }
        },
    }; // clang-format on
}

Value FuncGenContext::getValueImpl(ast::NonTrivAssignExpr const& expr) {
    if (auto* copyCtor = expr.copyCtor()) {
        /// We have an lvalue right hand side. If the values are different, we
        /// call the destructor of LHS and the copy constructor of LHS with RHS
        /// as argument. If the values are the same we do nothing
        auto dest = getValue(expr.dest());
        auto source = getValue(expr.source());
        SC_ASSERT(dest.isMemory(), "");
        SC_ASSERT(source.isMemory(), "");
        /// We branch here because the values might be the same.
        auto* addrEq = add<ir::CompareInst>(dest.get(),
                                            source.get(),
                                            ir::CompareMode::Unsigned,
                                            ir::CompareOperation::NotEqual,
                                            "addr.eq");
        auto* assignBlock = newBlock("assign");
        auto* endBlock = newBlock("assign.end");
        add<ir::Branch>(addrEq, assignBlock, endBlock);
        add(assignBlock);
        callDtor(expr.dest()->object(), expr.dtor());
        auto* function = getFunction(copyCtor);
        add<ir::Call>(function,
                      std::array{ dest.get(), source.get() },
                      std::string{});
        add<ir::Goto>(endBlock);
        add(endBlock);
    }
    else {
        /// We have an rvalue right hand side. We call the destructor of LHS and
        /// generate the value of RHS in place of LHS
        auto dest = getValue(expr.dest());
        SC_ASSERT(dest.isMemory(), "");
        callDtor(expr.dest()->object(), expr.dtor());
        auto source = getValue(expr.source());
        SC_ASSERT(source.isMemory(), "");
        SC_ASSERT(isa<ir::Alloca>(source.get()),
                  "How can an rvalue in memory not be stack memory?");
        source.get()->replaceAllUsesWith(dest.get());
    }
    return Value();
}

/// MARK: - General utilities

void FuncGenContext::callDtor(sema::Object const* object,
                              sema::Function const* dtor) {
    auto* function = getFunction(dtor);
    auto value = valueMap(object);
    SC_ASSERT(value.isMemory(),
              "Objects with non trivial lifetime must be in memory");
    add<ir::Call>(function, std::array{ value.get() }, std::string{});
}

void FuncGenContext::emitDtorCalls(sema::DTorStack const& dtorStack) {
    for (auto call: dtorStack) {
        callDtor(call.object, call.destructor);
    }
}

ir::Value* FuncGenContext::toRegister(Value value) {
    auto* semaType = typeMap(value.type());
    SC_ASSERT(!semaType || semaType->hasTrivialLifetime(),
              "We can only have trivial lifetime types in registers");
    switch (value.location()) {
    case Register:
        return value.get();
    case Memory:
        return add<ir::Load>(value.get(),
                             value.type(),
                             std::string(value.get()->name()));
    }
}

ir::Value* FuncGenContext::toMemory(Value value) {
    switch (value.location()) {
    case Register:
        return storeToMemory(value.get());

    case Memory:
        return value.get();
    }
}

ir::Value* FuncGenContext::toValueLocation(ValueLocation location,
                                           Value value) {
    switch (location) {
    case Register:
        return toRegister(value);
    case Memory:
        return toMemory(value);
    }
}