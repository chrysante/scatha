#include "IRGen/GenerateFunction.h"

#include <svm/Builtin.h>
#include <utl/function_view.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IRGen/Builder.h"
#include "IRGen/Globals.h"
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

struct FuncGenContext: FunctionBuilder {
    // Global references
    sema::Function const& semaFn;
    ir::Function& irFn;
    ir::Context& ctx;
    ir::Module& mod;
    sema::SymbolTable const& symbolTable;
    TypeMap const& typeMap;
    FunctionMap& functionMap;

    /// Local state
    ValueMap valueMap;
    utl::stack<Loop, 4> loopStack;

    FuncGenContext(sema::Function const& semaFn,
                   ir::Function& irFn,
                   ir::Context& ctx,
                   ir::Module& mod,
                   sema::SymbolTable const& symbolTable,
                   TypeMap const& typeMap,
                   FunctionMap& functionMap):
        FunctionBuilder(ctx, &irFn),
        semaFn(semaFn),
        irFn(irFn),
        ctx(ctx),
        mod(mod),
        symbolTable(symbolTable),
        typeMap(typeMap),
        functionMap(functionMap),
        valueMap(ctx) {}

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
    void emitDestructorCalls(sema::DTorStack const& dtorStack);

    /// Creates array size values and stores them in `objectMap` if declared
    /// type is array
    void generateDeclArraySizeImpl(
        ast::VarDeclBase const* varDecl,
        utl::function_view<ir::Value*()> sizeCallback);

    void generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                  sema::Object const* initObject);

    void generateParamArraySize(ast::VarDeclBase const* varDecl,
                                ir::Parameter* param);

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
    Value getValueImpl(ast::ConstructorCall const&);
    Value getValueImpl(ast::TrivialCopyExpr const&);

    /// # Expression specific utilities
    void generateArgument(PassingConvention const& PC,
                          Value arg,
                          sema::Object const* object,
                          utl::vector<ir::Value*>& outArgs);
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

    ///
    ir::Callable* getFunction(sema::Function const*);

    ///
    ir::ExtFunction* getMemcpy();

    ///
    CallingConvention const& getCC(sema::Function const*);
};

} // namespace

void irgen::generateFunction(ast::FunctionDefinition const& funcDecl,
                             ir::Function& irFn,
                             ir::Context& ctx,
                             ir::Module& mod,
                             sema::SymbolTable const& symbolTable,
                             TypeMap const& typeMap,
                             FunctionMap& functionMap) {

    FuncGenContext(*funcDecl.function(),
                   irFn,
                   ctx,
                   mod,
                   symbolTable,
                   typeMap,
                   functionMap)
        .generate(funcDecl);
}

/// MARK: - Statements

void FuncGenContext::generate(ast::Statement const& node) {
    visit(node, [this](auto const& node) { return generateImpl(node); });
}

void FuncGenContext::generateImpl(ast::CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    emitDestructorCalls(cmpStmt.dtorStack());
}

void FuncGenContext::generateImpl(ast::FunctionDefinition const& def) {
    addNewBlock("entry");
    auto const& CC = getCC(&semaFn);
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
    sema::QualType semaType = paramDecl->type();
    auto* irParam = irParamItr.to_address();
    auto* irType = typeMap(paramDecl->type());
    std::string name(paramDecl->name());
    auto* paramVar = paramDecl->variable();
    auto* baseType = stripRefOrPtr(semaType.get()).get();
    auto* arrayType = dyncast<sema::ArrayType const*>(baseType);
    bool const isDynArray = arrayType && arrayType->isDynamic();
    switch (pc.location()) {
    case Register: {
        if (isa<sema::ReferenceType>(*paramDecl->type())) {
            valueMap.insert(paramVar, Value(irParam, Register));
            ++irParamItr;
            if (isDynArray) {
                Value size(irParam->next(), Register);
                valueMap.insertArraySize(paramVar, size);
                ++irParamItr;
            }
            // FIXME: What about references to static arrays?
        }
        else {
            auto* address = storeToMemory(irParam, name);
            valueMap.insert(paramVar, Value(address, irType, Memory));
            ++irParamItr;
            if (isDynArray) {
                SC_ASSERT(isa<sema::PointerType>(*paramDecl->type()),
                          "Can't pass dynamic array by value");
                Value size(storeToMemory(irParam->next()),
                           irParam->next()->type(),
                           Memory);
                valueMap.insertArraySize(paramVar, size);
                ++irParamItr;
            }
            else if (arrayType) {
                auto* size = ctx.intConstant(arrayType->count(), 64);
                valueMap.insertArraySize(paramVar, Value(size, Register));
            }
        }
        break;
    }

    case Memory: {
        Value data(irParam, irType, Memory);
        valueMap.insert(paramVar, data);
        ++irParamItr;
        if (arrayType) {
            SC_ASSERT(!isDynArray,
                      "By value array parameters cannot be dynamic");
            auto* size = ctx.intConstant(arrayType->count(), 64);
            valueMap.insertArraySize(paramVar, Value(size, Register));
        }
        break;
    }
    }
}

void FuncGenContext::generateDeclArraySizeImpl(
    ast::VarDeclBase const* varDecl,
    utl::function_view<ir::Value*()> sizeCallback) {
    auto* type = stripRefOrPtr(varDecl->type().get()).get();
    auto* arrayType = dyncast<sema::ArrayType const*>(type);
    if (!arrayType) {
        return;
    }
    if (!arrayType->isDynamic()) {
        valueMap.insertArraySize(varDecl->variable(), arrayType->count());
    }
    else if (sema::isRef(varDecl->type())) {
        valueMap.insertArraySize(varDecl->variable(),
                                 Value(sizeCallback(), Register));
    }
    else {
        auto* size = sizeCallback();
        auto* sizeVar =
            storeToMemory(size, utl::strcat(varDecl->name(), ".size"));
        valueMap.insertArraySize(varDecl->variable(),
                                 Value(sizeVar, size->type(), Memory));
    }
}

void FuncGenContext::generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                              sema::Object const* initObject) {
    generateDeclArraySizeImpl(varDecl, [&] {
        return toRegister(valueMap.arraySize(initObject));
    });
}

void FuncGenContext::generateParamArraySize(ast::VarDeclBase const* varDecl,
                                            ir::Parameter* param) {
    generateDeclArraySizeImpl(varDecl, [&] { return param->next(); });
}

static bool varDeclNeedCopy(sema::QualType type) {
    return type->hasTrivialLifetime() && !isa<sema::ArrayType>(*type);
}

void FuncGenContext::generateImpl(ast::VariableDeclaration const& varDecl) {
    auto dtorStack = varDecl.dtorStack();
    std::string name = std::string(varDecl.name());
    auto* initExpr = varDecl.initExpression();
    if (sema::isRef(varDecl.type())) {
        SC_ASSERT(initExpr, "Reference must be initialized");
        auto value = getValue(initExpr);
        valueMap.insert(varDecl.variable(), value);
        /// We don't store array size because we just reuse the value from our
        /// init expression, so the array size is already stored
    }
    else if (initExpr) {
        auto value = getValue(initExpr);
        ir::Value* address = nullptr;
        /// The test for trivial lifetime is temporary. We should find a better
        /// solution but for now it works. It works because for trivial lifetime
        /// types
        if (value.isMemory() && initExpr->isRValue() &&
            !varDeclNeedCopy(initExpr->type()))
        {
            address = value.get();
            address->setName(name);
        }
        else {
            address = storeToMemory(toRegister(value), name);
        }
        valueMap.insert(varDecl.variable(),
                        Value(address, value.type(), Memory));
        generateVarDeclArraySize(&varDecl, initExpr->object());
    }
    else {
        auto* type = typeMap(varDecl.type());
        auto* address = makeLocalVariable(type, name);
        valueMap.insert(varDecl.variable(), Value(address, type, Memory));
        generateVarDeclArraySize(&varDecl, nullptr);
    }
    emitDestructorCalls(dtorStack);
}

void FuncGenContext::generateImpl(
    ast::ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    emitDestructorCalls(exprStatement.dtorStack());
}

void FuncGenContext::generateImpl(ast::ReturnStatement const& retDecl) {
    if (!retDecl.expression()) {
        add<ir::Return>(ctx.voidValue());
        return;
    }
    auto returnValue = getValue(retDecl.expression());
    emitDestructorCalls(retDecl.dtorStack());
    // clang-format off
    SC_MATCH (*stripRefOrPtr(retDecl.expression()->type())) {
        [&](sema::ObjectType const&) {
            auto const& CC = getCC(&semaFn);
            switch (CC.returnValue().location()) {
            case Register:
                add<ir::Return>(toRegister(returnValue));
                break;
                
            case Memory:
                add<ir::Store>(&irFn.parameters().front(),
                               toRegister(returnValue));
                add<ir::Return>(ctx.voidValue());
                break;
            }
        },
        [&](sema::ArrayType const&) {
            auto const& CC = getCC(&semaFn);
            switch (CC.returnValue().location()) {
            case Register: {
                auto size = valueMap.arraySize(retDecl.expression()->object());
                auto* baseValue = ctx.undef(makeArrayViewType(ctx));
                auto* insertData = add<ir::InsertValue>(baseValue,
                                                        toRegister(returnValue),
                                                        std::array{ size_t{ 0 } },
                                                        "retval");
                auto* insertSize = add<ir::InsertValue>(insertData,
                                                        toRegister(size),
                                                        std::array{ size_t{ 1 } },
                                                        "retval");
                add<ir::Return>(insertSize);
                break;
            }
            case Memory:
                SC_UNIMPLEMENTED();
                break;
            }
        },
    }; // clang-format on
}

void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
    auto* condition = getValue<Register>(stmt.condition());
    emitDestructorCalls(stmt.dtorStack());
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
        emitDestructorCalls(loopStmt.conditionDtorStack());
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
        emitDestructorCalls(loopStmt.incrementDtorStack());
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
        emitDestructorCalls(loopStmt.conditionDtorStack());
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
        emitDestructorCalls(loopStmt.conditionDtorStack());
        add<ir::Branch>(condition, loopBody, loopEnd);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }
    }
    emitDestructorCalls(loopStmt.dtorStack());
}

void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
    emitDestructorCalls(jump.dtorStack());
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
    return Value(value.get(), Register);
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
        auto data = Value(staticData.get(), staticData.get()->type(), Register);
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
        ir::Value* opAddr = toRegister(operand);
        ir::Type const* operandType =
            typeMap(sema::stripReference(expr.operand()->type()));
        ir::Value* operandValue =
            add<ir::Load>(opAddr,
                          operandType,
                          utl::strcat(expr.operation(), ".op"));
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
        case ast::UnaryOperatorNotation::_count:
            SC_UNREACHABLE();
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

Value FuncGenContext::getValueImpl(ast::BinaryExpression const& expr) {
    auto* builtinType = dyncast<sema::BuiltinType const*>(
        sema::stripReference(expr.lhs()->type()).get());

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
        auto* type = lhs->type();
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
        auto operation = mapArithmeticOp(builtinType, expr.operation());
        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, "expr");
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
                                            "cmp.res");
        return Value(result, Register);
    }

    case Comma:
        getValue(expr.lhs());
        return getValue(expr.rhs());

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
        auto lhs = getValue(expr.lhs());
        auto rhs = getValue(expr.rhs());
        auto* rhsValue = toRegister(rhs);
        if (expr.operation() != Assignment) {
            SC_ASSERT(builtinType == expr.rhs()->type().get(), "");
            auto operation =
                mapArithmeticAssignOp(builtinType, expr.operation());
            rhsValue = add<ir::ArithmeticInst>(toRegister(lhs),
                                               rhsValue,
                                               operation,
                                               "expr");
        }
        add<ir::Store>(lhs.get(), rhsValue);
        auto* arrayType = ptrToArray(expr.lhs()->type().get());
        if (arrayType && arrayType->isDynamic()) {
            SC_ASSERT(expr.operation() == Assignment, "");
            auto lhsSize = valueMap.arraySize(expr.lhs()->object());
            SC_ASSERT(lhsSize.location() == Memory,
                      "Must be in memory to reassign");
            auto rhsSize = valueMap.arraySize(expr.rhs()->object());
            add<ir::Store>(lhsSize.get(), toRegister(rhsSize));
        }
        return Value();
    }
    case _count:
        SC_UNREACHABLE();
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
#warning do we need this branch?
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
        auto* baseType = typeMap(sema::stripReference(expr.accessed()->type()));
        auto* result = add<ir::GetElementPointer>(baseType,
                                                  base.get(),
                                                  ctx.intConstant(0, 64),
                                                  std::array{ irIndex },
                                                  "mem.acc");
        if (sema::isRef(expr.type())) {
            value = Value(result, Register);
        }
        else {
            auto* accessedType = typeMap(var->type());
            value = Value(result, accessedType, Memory);
        }
        break;
    }
    }
    sema::QualType memType = expr.type();
    auto* arrayType = ptrToArray(stripReference(memType).get());
    if (!arrayType) {
        return value;
    }
    auto lazySize = [this, base, sizeIndex = irIndex + 1] {
        switch (base.location()) {
        case Register: {
            auto* result = add<ir::ExtractValue>(base.get(),
                                                 std::array{ sizeIndex },
                                                 "mem.acc.size");
            return Value(result, Register);
        }
        case Memory: {
            auto* result = add<ir::GetElementPointer>(base.type(),
                                                      base.get(),
                                                      ctx.intConstant(0, 64),
                                                      std::array{ sizeIndex },
                                                      "mem.acc.size");
            return Value(result, ctx.intType(64), Memory);
        }
        }
    };
    valueMap.insertArraySize(expr.object(), lazySize);
    return value;
}

Value FuncGenContext::getValueImpl(ast::DereferenceExpression const& expr) {
    /// Since a dereference expression converts from `*T` to `&T`, this is a
    /// no-op. The actual dereferencing happens in the conversion nodes.
    return getValue(expr.referred());
}

Value FuncGenContext::getValueImpl(ast::AddressOfExpression const& expr) {
    /// See `getValueImpl(DereferenceExpression)`
    return getValue(expr.referred());
}

Value FuncGenContext::getValueImpl(ast::Conditional const& condExpr) {
    auto* cond = getValue<Register>(condExpr.condition());
    auto* thenBlock = newBlock("cond.then");
    auto* elseBlock = newBlock("cond.else");
    auto* endBlock = newBlock("cond.end");
    add<ir::Branch>(cond, thenBlock, elseBlock);

    /// Generate then block.
    add(thenBlock);
    auto* thenVal = getValue<Register>(condExpr.thenExpr());
    thenBlock = &currentBlock(); /// Nested `?:` operands etc. may have changed
                                 /// `currentBlock`
    add<ir::Goto>(endBlock);

    /// Generate else block.
    add(elseBlock);
    auto* elseVal = getValue<Register>(condExpr.elseExpr());
    elseBlock = &currentBlock();
    add<ir::Goto>(endBlock);

    /// Generate end block.
    add(endBlock);
    std::array<ir::PhiMapping, 2> phiArgs = { { { thenBlock, thenVal },
                                                { elseBlock, elseVal } } };
    auto* result = add<ir::Phi>(phiArgs, "cond");
    return Value(result, Register);
}

Value FuncGenContext::getValueImpl(ast::FunctionCall const& call) {
    ir::Callable* function = getFunction(call.function());
    auto const& CC = getCC(call.function());
    utl::small_vector<ir::Value*> arguments;
    auto const retvalLocation = CC.returnValue().location();
    if (retvalLocation == Memory) {
        auto* returnType = typeMap(call.function()->returnType());
        arguments.push_back(makeLocalVariable(returnType, "retval"));
    }
    for (auto [PC, arg]: ranges::views::zip(CC.arguments(), call.arguments())) {
        generateArgument(PC, getValue(arg), arg->object(), arguments);
    }
    bool callHasName = !isa<ir::VoidType>(function->returnType());
    std::string name = callHasName ? "call.result" : std::string{};
    auto* inst = add<ir::Call>(function, arguments, std::move(name));
    // clang-format off
    Value value = SC_MATCH(*stripRefOrPtr(call.type())) {
        [&](sema::ObjectType const&) {
            switch (retvalLocation) {
            case Register:
                return Value(inst, Register);
            case Memory:
                return Value(
                             arguments.front(),
                             typeMap(call.function()->returnType()),
                             Memory);
            }
        },
        [&](sema::ArrayType const&) {
            switch (retvalLocation) {
            case Register: {
                auto* data =
                    add<ir::ExtractValue>(inst,
                                          std::array{ size_t{ 0 } }, "data");
                auto* size =
                    add<ir::ExtractValue>(inst,
                                          std::array{ size_t{ 1 } }, "size");
                Value value(data, Register);
                valueMap.insertArraySize(call.object(), Value(size, Register));
                return value;
            }
            case Memory:
                SC_UNIMPLEMENTED();
            }
        },
    }; // clang-format on
    valueMap.insert(call.object(), value);
    return value;
}

void FuncGenContext::generateArgument(PassingConvention const& PC,
                                      Value value,
                                      sema::Object const* object,
                                      utl::vector<ir::Value*>& arguments) {
    arguments.push_back(toValueLocation(PC.location(), value));
    if (PC.numParams() == 2) {
        arguments.push_back(toRegister(valueMap.arraySize(object)));
    }
}

Value FuncGenContext::getValueImpl(ast::Subscript const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(
        stripReference(expr.callee()->type()).get());
    auto* elemType = typeMap(arrayType->elementType());
    auto array = getValue(expr.callee());
    /// Right now we don't use the size but here we could issue a call to an
    /// assertion function
    [[maybe_unused]] auto size = valueMap.arraySize(expr.callee()->object());
    auto index = getValue<Register>(expr.arguments().front());
    switch (array.location()) {
    case Register:
        /// Here we should insert `ExtractValue` instruction
        SC_UNIMPLEMENTED();
    case Memory: {
        auto* addr = add<ir::GetElementPointer>(elemType,
                                                array.get(),
                                                index,
                                                std::initializer_list<size_t>{},
                                                "elem.ptr");
        return Value(addr, elemType, Register);
    }
    }
}

Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(
        stripReference(expr.callee()->type()).get());
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
    Value result(addr, Register);
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
    auto constData =
        allocate<ir::ConstantData>(ctx,
                                   ctx.arrayType(typeMap(elemType),
                                                 list.elements().size()),
                                   std::move(data),
                                   "array");
    auto* source = constData.get();
    mod.addConstantData(std::move(constData));
    auto* memcpy = getMemcpy();
    auto* size = ctx.intConstant(list.elements().size() * elemType->size(), 64);
    std::array<ir::Value*, 4> args = { dest, size, source, size };
    add<ir::Call>(memcpy, args, std::string{});
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

Value FuncGenContext::getValueImpl(ast::Conversion const& conv) {
    auto* expr = conv.expression();
    Value refConvResult = [&]() -> Value {
        switch (conv.conversion()->refConversion()) {
        case sema::RefConversion::None:
            return getValue(expr);

        case sema::RefConversion::Dereference: {
            auto address = getValue(expr);
            return Value(toRegister(address),
                         typeMap(stripReference(expr->type())),
                         Memory);
        }

        case sema::RefConversion::MaterializeTemporary: {
            auto value = getValue(expr);
            if (value.isMemory()) {
                return Value(value.get(), Register);
            }
            else {
                auto* temp = storeToMemory(value.get());
                return Value(temp, Register);
            }
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
        auto* fromType = ptrToArray(sema::stripReference(expr->type()).get());
        SC_ASSERT(fromType, "");
        auto* toType = ptrToArray(conv.type().get());
        SC_ASSERT(toType, "");
        auto data = refConvResult;
        if (toType->isDynamic()) {
            if (fromType->isDynamic()) {
                auto count = valueMap.arraySize(expr->object());
                if (conv.conversion()->objectConversion() ==
                    Reinterpret_Array_ToByte)
                {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                ctx.intConstant(8, 64),
                                                ir::ArithmeticOperation::Mul,
                                                "reinterpret.count");
                    count = Value(newCount, Register);
                }
                else {
                    auto* newCount =
                        add<ir::ArithmeticInst>(toRegister(count),
                                                ctx.intConstant(8, 64),
                                                ir::ArithmeticOperation::SDiv,
                                                "reinterpret.count");
                    count = Value(newCount, Register);
                }
                valueMap.insertArraySize(conv.object(), count);
                return data;
            }
            size_t count = fromType->count();
            switch (conv.conversion()->objectConversion()) {
            case Reinterpret_Array_ToByte:
                count *= 8;
                break;
            case Reinterpret_Array_FromByte:
                count /= 8;
                break;
            default:
                SC_UNREACHABLE();
            }
            valueMap.insertArraySize(conv.object(), count);
            return data;
        }
        SC_ASSERT(!fromType->isDynamic(), "Invalid conversion");
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

Value FuncGenContext::getValueImpl(ast::ConstructorCall const& call) {
    using enum sema::SpecialMemberFunction;
    switch (call.kind()) {
    case New: {
        auto* type = typeMap(call.constructedType());
        auto* address = makeLocalVariable(type, "anon");
        auto* function = getFunction(call.function());
        auto const& CC = getCC(call.function());
        /// Lifetime function always must take the object parameter by reference
        /// so we can just pass the pointer here
        utl::small_vector<ir::Value*> arguments = { address };
        auto PCsAndArgs =
            ranges::views::zip(CC.arguments() | ranges::views::drop(1),
                               call.arguments());
        for (auto [PC, arg]: PCsAndArgs) {
            generateArgument(PC, getValue(arg), arg->object(), arguments);
        }
        valueMap.insert(call.object(), Value(address, type, Memory));
        add<ir::Call>(function, arguments, std::string{});
        return Value(address, type, Memory);
    }
    case Move:
        SC_UNIMPLEMENTED();
    default:
        SC_UNREACHABLE();
    }
}

Value FuncGenContext::getValueImpl(ast::TrivialCopyExpr const& expr) {
    // clang-format off
    return SC_MATCH (*expr.type()) {
        [&](sema::ObjectType const& type) {
            auto value = getValue(expr.argument());
            auto result = Value(toRegister(value), Register);
            auto arraySize = valueMap.tryGetArraySize(expr.argument()->object());
            if (arraySize) {
                auto newSize = Value(toRegister(*arraySize), Register);
                valueMap.insertArraySize(expr.object(), newSize);
            }
            return result;
        },
        [&](sema::ArrayType const& type) -> Value {
            if (type.size() <= 64) {
                auto value = getValue(expr.argument());
                return Value(toRegister(value), Register);
            }
            else {
                auto source = getValue(expr.argument());
                SC_ASSERT(source.isMemory(), "");
                auto* arrayType = typeMap(&type);
                auto* array = makeLocalVariable(arrayType, "list");
                auto* memcpy = getMemcpy();
                auto* size = ctx.intConstant(type.size(), 64);
                std::array<ir::Value*, 4> args = { array, size, source.get(), size };
                add<ir::Call>(memcpy, args, std::string{});
                return Value(array, arrayType, Memory);
            }
        },
    }; // clang-format on
}

/// MARK: - General utilities

void FuncGenContext::emitDestructorCalls(sema::DTorStack const& dtorStack) {
    for (auto call: dtorStack) {
        auto* function = getFunction(call.destructor);
        auto object = valueMap(call.object);
        SC_ASSERT(object.isMemory(),
                  "Objects with non trivial lifetime must be in memory");
        add<ir::Call>(function, std::array{ object.get() }, std::string{});
    }
}

ir::Value* FuncGenContext::toRegister(Value value) {
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

ir::Callable* FuncGenContext::getFunction(sema::Function const* semaFunction) {
    switch (semaFunction->kind()) {
    case sema::FunctionKind::Native:
        return functionMap(semaFunction);

    case sema::FunctionKind::External:
        [[fallthrough]];
    case sema::FunctionKind::Generated:
        if (auto* irFunction = functionMap.tryGet(semaFunction)) {
            return irFunction;
        }
        return declareFunction(semaFunction, ctx, mod, typeMap, functionMap);
    }
}

ir::ExtFunction* FuncGenContext::getMemcpy() {
    size_t index = static_cast<size_t>(svm::Builtin::memcpy);
    auto* semaMemcpy = symbolTable.builtinFunction(index);
    auto* irMemcpy = getFunction(semaMemcpy);
    return cast<ir::ExtFunction*>(irMemcpy);
}

CallingConvention const& FuncGenContext::getCC(sema::Function const* function) {
    return functionMap.metaData(function).CC;
}
