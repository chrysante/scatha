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
#include "Sema/Analysis/Utility.h"
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
    void generateImpl(ast::ImportStatement const&);
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
    void generateCleanup(sema::Object const* object,
                         sema::LifetimeOperation destroy,
                         ast::ASTNode const& sourceNode);
    void generateCleanups(sema::CleanupStack const& cleanupStack,
                          ast::ASTNode const& sourceNode);

    /// Creates array size values and stores them in `objectMap` if declared
    /// type is array
    void generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                  sema::Object const* initObject);

    /// # Expressions
    SC_NODEBUG Value getValue(ast::Expression const* expr);

    /// \Returns single value if `Repr == Packed` or vector of values if
    /// `Repr == Unpacked`
    template <ValueRepresentation Repr>
    SC_NODEBUG auto getValue(ValueLocation loc, ast::Expression const* expr) {
        return to<Repr>(loc, getValue(expr));
    }

    /// \Returns a _lazy_ view of the generated values of the expressions in the
    /// range \p expressions
    auto getValues(auto&& expressions) {
        return expressions |
               transform([&](auto* expr) { return getValue(expr); });
    }

    Value getValueImpl(ast::Expression const&) { SC_UNREACHABLE(); }
    Value getValueImpl(ast::Identifier const&);
    Value getValueImpl(ast::Literal const&);
    Value getValueImpl(ast::UnaryExpression const&);
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
    utl::small_vector<ir::Value*> unpackArguments(auto&& passingConventions,
                                                  auto&& values);
    Value getValueImpl(ast::Subscript const&);
    Value getValueImpl(ast::SubscriptSlice const&);
    Value getValueImpl(ast::ListExpression const&);
    bool genStaticListData(ast::ListExpression const& list, ir::Alloca* dest);
    void genDynamicListData(ast::ListExpression const& list, ir::Alloca* dest);
    // Value getValueImpl(ast::MoveExpr const&);
    // Value getValueImpl(ast::UniqueExpr const&);
    //    Value getValueImpl(ast::Conversion const&);
    // Value getValueImpl(ast::UninitTemporary const&);

    Value getValueImpl(ast::ValueCatConvExpr const&);
    Value getValueImpl(ast::MutConvExpr const&);
    Value getValueImpl(ast::ObjTypeConvExpr const&);

    Value getValueImpl(ast::ConstructExpr const&);

    Value getValueImpl(ast::TrivDefConstructExpr const&);
    Value getValueImpl(ast::TrivCopyConstructExpr const&);
    Value getValueImpl(ast::TrivAggrConstructExpr const&);
    Value getValueImpl(ast::NontrivConstructExpr const&);
    Value getValueImpl(ast::NontrivAggrConstructExpr const&);

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

void FuncGenContext::generateImpl(ast::ImportStatement const&) {
    /// No-op
}

void FuncGenContext::generateImpl(ast::CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    generateCleanups(cmpStmt.cleanupStack(), cmpStmt);
}

static std::optional<sema::SMFKind> prologueAsSMF(sema::Function const& F) {
    using enum sema::SMFKind;
    if (F.name() == "new" || F.name() == "move") {
        return DefaultConstructor;
    }
    return std::nullopt;
}

static std::optional<sema::SMFKind> epilogueAsSMF(sema::Function const& F) {
    using enum sema::SMFKind;
    if (auto kind = F.smfKind(); kind && *kind == Destructor) {
        return Destructor;
    }
    return std::nullopt;
}

void FuncGenContext::generateImpl(ast::FunctionDefinition const& def) {
    /// If the function is a user defined special member function (constructor
    /// or destructor) we still generate the code of the non-user defined
    /// equivalent function and then append the user defined code. This way in a
    /// user defined destructor all member destructors get called and in a user
    /// defined constructor all member variables get initialized automatically
    if (auto kind = prologueAsSMF(semaFn)) {
        generateSynthFunctionAs(*kind, config, *this);
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
    if (auto kind = epilogueAsSMF(semaFn)) {
        generateSynthFunctionAs(*kind, config, *this);
        makeBlockCurrent(&irFn.back());
    }
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

void FuncGenContext::generateImpl(ast::VariableDeclaration const& varDecl) {
    auto cleanupStack = varDecl.cleanupStack();
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
    generateCleanups(cleanupStack, varDecl);
}

void FuncGenContext::generateImpl(
    ast::ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    generateCleanups(exprStatement.cleanupStack(), exprStatement);
}

void FuncGenContext::generateImpl(ast::ReturnStatement const& retStmt) {
    /// Simple case of `return;` in a void function
    if (!retStmt.expression()) {
        generateCleanups(retStmt.cleanupStack(), retStmt);
        add<ir::Return>(ctx.voidValue());
        return;
    }

    /// More complex `return <value>;` case
    auto retval = getValue(retStmt.expression());
    auto CC = getCC(&semaFn);
    switch (CC.returnValue().location()) {
    case Register: {
        /// Pointers we keep in registers but references directly refer to the
        /// value in memory
        auto destLocation = CC.returnValue().locationAtCallsite();
        auto* value = to<Packed>(destLocation, retval);
        generateCleanups(retStmt.cleanupStack(), retStmt);
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
        generateCleanups(retStmt.cleanupStack(), retStmt);
        add<ir::Return>(ctx.voidValue());
        return;
    }
    }
}

void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
    auto* condition = getValue<Packed>(Register, stmt.condition());
    generateCleanups(stmt.cleanupStack(), stmt);
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
        auto* condition = getValue<Packed>(Register, loopStmt.condition());
        generateCleanups(loopStmt.conditionDtorStack(), loopStmt);
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
        generateCleanups(loopStmt.incrementDtorStack(), loopStmt);
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
        auto* condition = getValue<Packed>(Register, loopStmt.condition());
        generateCleanups(loopStmt.conditionDtorStack(), loopStmt);
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
        auto* condition = getValue<Packed>(Register, loopStmt.condition());
        generateCleanups(loopStmt.conditionDtorStack(), loopStmt);
        add<ir::Branch>(condition, loopBody, loopEnd);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }
    }
    generateCleanups(loopStmt.cleanupStack(), loopStmt);
}

void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
    generateCleanups(jump.cleanupStack(), jump);
    auto* dest = [&] {
        auto& currentLoop = loopStack.top();
        switch (jump.kind()) {
        case ast::JumpStatement::Break:
            return currentLoop.end;
        case ast::JumpStatement::Continue:
            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
        }
        SC_UNREACHABLE();
    }();
    add<ir::Goto>(dest);
}

/// MARK: - Expressions

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
        return Value(name,
                     lit.type().get(),
                     { global, ctx.intConstant(text.size(), 64) },
                     Memory,
                     Unpacked);
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

Value FuncGenContext::getValueImpl(ast::UnaryExpression const& expr) {
    using enum ast::UnaryOperator;
    using enum ir::ArithmeticOperation;
    switch (expr.operation()) {
    case Increment:
        [[fallthrough]];
    case Decrement: {
        Value operand = getValue(expr.operand());
        SC_ASSERT(operand.isMemory(),
                  "Operand must be in memory to be modified");
        ir::Value* opAddr = to<Packed>(Memory, operand);
        ir::Type const* operandType =
            typeMap.packed(expr.operand()->type().get());
        ir::Value* operandValue = to<Packed>(Register, operand);
        auto* newValue =
            add<ir::ArithmeticInst>(operandValue,
                                    ctx.arithmeticConstant(1, operandType),
                                    expr.operation() == Increment ? Add : Sub,
                                    utl::strcat(expr.operation(), ".res"));
        add<ir::Store>(opAddr, newValue);
        switch (expr.notation()) {
        case ast::UnaryOperatorNotation::Prefix:
            return operand;
        case ast::UnaryOperatorNotation::Postfix:
            return Value(operand.name(),
                         expr.type().get(),
                         { operandValue },
                         Register,
                         Packed);
        }
    }

    case ast::UnaryOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryOperator::Negation: {
        auto* operand = getValue<Packed>(Register, expr.operand());
        auto operation =
            isa<sema::IntType>(expr.operand()->type().get()) ? Sub : FSub;
        auto* newValue =
            add<ir::ArithmeticInst>(ctx.arithmeticConstant(0, operand->type()),
                                    operand,
                                    operation,
                                    "negated");
        return Value("negated",
                     expr.type().get(),
                     { newValue },
                     Register,
                     Packed);
    }

    default:
        auto* operand = getValue<Packed>(Register, expr.operand());
        auto* newValue =
            add<ir::UnaryArithmeticInst>(operand,
                                         mapUnaryOp(expr.operation()),
                                         "expr");
        return Value("expr", expr.type().get(), { newValue }, Register, Packed);
    }
}

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
        auto* lhs = getValue<Packed>(Register, expr.lhs());
        auto* rhs = getValue<Packed>(Register, expr.rhs());
        auto operation = mapArithmeticOp(type, expr.operation());
        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, resName);
        return Value(resName, expr.type().get(), { result }, Register, Packed);
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue<Packed>(Register, expr.lhs());
        SC_ASSERT(lhs->type() == ctx.boolType(),
                  "Need i1 for logical operation");
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
        auto* rhs = getValue<Packed>(Register, expr.rhs());
        SC_ASSERT(rhs->type() == ctx.boolType(),
                  "Need i1 for logical operation");
        add<ir::Goto>(endBlock);
        add(endBlock);

        auto* result = [&] {
            if (expr.operation() == LogicalAnd) {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, ctx.boolConstant(0) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.and");
            }
            else {
                return add<ir::Phi>(
                    std::array<ir::PhiMapping, 2>{
                        ir::PhiMapping{ startBlock, ctx.boolConstant(1) },
                        ir::PhiMapping{ rhsBlock, rhs } },
                    "log.or");
            }
        }();
        return Value("log.or", expr.type().get(), { result }, Register, Packed);
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
        auto lhs = getValue<Unpacked>(Register, expr.lhs());
        auto rhs = getValue<Unpacked>(Register, expr.rhs());
        auto values = zip(lhs, rhs) | transform([&](auto p) -> ir::Value* {
            auto [lhs, rhs] = p;
            return add<ir::CompareInst>(lhs,
                                        rhs,
                                        mapCompareMode(type),
                                        mapCompareOp(expr.operation()),
                                        resName);
        }) | ToSmallVector<>;
        using enum ir::ArithmeticOperation;
        auto* combined =
            foldValues(expr.operation() == Equals ? And : Or, values, resName);
        return Value(resName,
                     expr.type().get(),
                     { combined },
                     Register,
                     Packed);
    }

    case Comma:
        (void)getValue(expr.lhs());  /// Evaluate and discard LHS
        return getValue(expr.rhs()); /// Evaluate and return RHS

    case Assignment: {
        auto* lhs = getValue<Packed>(Memory, expr.lhs());
        /// TODO: Don't load to register unconditionally.
        /// Use `memcpy` for large types
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
        auto lhs = getValue(expr.lhs());
        SC_ASSERT(lhs.isMemory(), "Must be in memory to assign");
        auto rhs = getValue<Packed>(Register, expr.rhs());
        auto operation = mapArithmeticAssignOp(type, expr.operation());
        auto* exprRes = add<ir::ArithmeticInst>(to<Packed>(Register, lhs),
                                                rhs,
                                                operation,
                                                resName);
        add<ir::Store>(toUnpackedMemory(lhs).front(), exprRes);
        return Value("", expr.type().get(), {}, Register, Packed);
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
        auto* arrayType =
            cast<sema::ArrayType const*>(expr.accessed()->type().get());
        auto accessed = getValue(expr.accessed());
        auto* empty = [&]() -> ir::Value* {
            if (!arrayType->isDynamic()) {
                return ctx.boolConstant(arrayType->count());
            }
            auto* size = to<Packed>(Register,
                                    getArraySize(expr.accessed()->type().get(),
                                                 accessed));
            return add<ir::CompareInst>(size,
                                        ctx.intConstant(0, 64),
                                        ir::CompareMode::Signed,
                                        ir::CompareOperation::Equal,
                                        "empty");
        }();
        return Value("empty", expr.type().get(), { empty }, Register, Packed);
    }
    case ArrayFront:
        [[fallthrough]];
    case ArrayBack: {
        // TODO: Check that array is not empty
        auto* arrayType =
            cast<sema::ArrayType const*>(expr.accessed()->type().get());
        bool isFront = prop.kind() == ArrayFront;
        auto accessed = getValue(expr.accessed());
        auto name = utl::strcat(accessed.name(), isFront ? ".front" : ".back");
        switch (accessed.location()) {
        case Register: {
            size_t index = isFront ? 0 : arrayType->count() - 1;
            auto* elem = add<ir::ExtractValue>(to<Packed>(Register, accessed),
                                               IndexArray{ index },
                                               name);
            return Value(name, expr.type().get(), { elem }, Register, Packed);
        }
        case Memory: {
            auto* index = [&]() -> ir::Value* {
                if (isFront) {
                    return ctx.intConstant(0, 64);
                }
                if (!arrayType->isDynamic()) {
                    return ctx.intConstant(arrayType->count() - 1, 64);
                }
                auto count =
                    getArraySize(expr.accessed()->type().get(), accessed);
                return add<ir::ArithmeticInst>(to<Packed>(Register, count),
                                               ctx.intConstant(1, 64),
                                               ir::ArithmeticOperation::Sub,
                                               "back.index");
            }();
            auto* irElemType = typeMap.packed(expr.type().get());
            auto* elem =
                add<ir::GetElementPointer>(irElemType,
                                           to<Unpacked>(Memory, accessed)
                                               .front(),
                                           index,
                                           IndexArray{},
                                           utl::strcat(name, ".addr"));
            return Value(name, expr.type().get(), { elem }, Memory, Packed);
        }
        }
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
    auto CC = getCC(call.function());
    auto retvalLocation = CC.returnValue().location();
    auto irArguments =
        unpackArguments(CC.arguments(), getValues(call.arguments()));
    /// Allocate return value storage
    if (retvalLocation == Memory) {
        auto* irReturnType = typeMap.packed(call.function()->returnType());
        irArguments.insert(irArguments.begin(),
                           makeLocalVariable(irReturnType,
                                             utl::strcat(name, ".addr")));
    }
    auto* callInst = add<ir::Call>(function, irArguments, name);
    auto* retval = retvalLocation == Memory ? irArguments.front() : callInst;
    return Value(name,
                 call.type().get(),
                 { retval },
                 CC.returnValue().locationAtCallsite(),
                 Packed);
}

utl::small_vector<ir::Value*> FuncGenContext::unpackArguments(
    auto&& passingConventions, auto&& values) {
    utl::small_vector<ir::Value*> irArguments;
    for (auto [PC, value]: zip(passingConventions, values)) {
        auto unpacked = to<Unpacked>(PC.locationAtCallsite(), value);
        SC_ASSERT(PC.numParams() == unpacked.size(), "Argument count mismatch");
        irArguments.insert(irArguments.end(), unpacked.begin(), unpacked.end());
    }
    return irArguments;
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

Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
    auto* elemType = typeMap.packed(arrayType->elementType());
    auto array = getValue(expr.callee());
    auto lower = getValue<Packed>(Register, expr.lower());
    auto upper = getValue<Packed>(Register, expr.upper());
    auto name = utl::strcat(array.name(), ".slice");
    auto* addr = add<ir::GetElementPointer>(elemType,
                                            to<Unpacked>(Memory, array).front(),
                                            lower,
                                            IndexArray{},
                                            utl::strcat(name, ".addr"));
    auto* size = add<ir::ArithmeticInst>(upper,
                                         lower,
                                         ir::ArithmeticOperation::Sub,
                                         utl::strcat(name, ".count"));
    return Value(name, expr.type().get(), { addr, size }, Memory, Unpacked);
}

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

Value FuncGenContext::getValueImpl(ast::ValueCatConvExpr const& conv) {
    auto value = getValue(conv.expression());
    using enum sema::ValueCatConversion;
    switch (conv.conversion()) {
    case LValueToRValue: {
        auto repr = value.representation();
        if (value.type()->size() <= PreferredMaxRegisterValueSize) {
            return Value(value.name(),
                         value.type(),
                         to(Register, repr, value),
                         Register,
                         repr);
        }
        else {
            auto* irType = typeMap.packed(value.type());
            auto* mem = makeLocalVariable(irType, value.name());
            callMemcpy(mem, toPackedMemory(value), irType->size());
            return Value(value.name(), value.type(), { mem }, Memory, Packed);
        }
    }

    case MaterializeTemporary:
        return Value(value.name(),
                     value.type(),
                     to(Memory, value.representation(), value),
                     Memory,
                     value.representation());
    }
}

Value FuncGenContext::getValueImpl(ast::MutConvExpr const& conv) {
    /// Mutability conversions are meaningless in IR
    return getValue(conv.expression());
}

Value FuncGenContext::getValueImpl(ast::ObjTypeConvExpr const& conv) {
    auto* expr = conv.expression();
    auto value = getValue(expr);
    using enum sema::ObjectTypeConversion;
    switch (conv.conversion()) {
    case NullptrToRawPtr:
        return Value(value.name(),
                     conv.type().get(),
                     { value.get(0), ctx.intConstant(0, 64) },
                     Register,
                     Unpacked);
    case NullptrToUniquePtr: {
        /// Here we have to consider if we want to keep unique pointers in
        /// memory all the time or if it is okay to have them in registers
        SC_UNIMPLEMENTED();
    }
    case UniqueToRawPtr:
        return value;
    case ArrayPtr_FixedToDynamic:
        [[fallthrough]];
    case ArrayRef_FixedToDynamic: {
        /// It would be nicer to have two cases `Array_FixedToDynamic` and
        /// `ArrayPointer_FixedToDynamic` but refactoring sema conversion is a
        /// major project that has to be done later
        SC_ASSERT(
            isa<sema::PointerType>(*expr->type()) || value.isMemory(),
            "Dynamic arrays cannot be in registers. For rvalues we should have a MaterializeTemporary conversion before this case");
        ValueLocation loc = value.location();
        auto* data = to(loc, Unpacked, value).front();
        size_t count = getStaticArraySize(stripPtr(expr->type().get())).value();
        return Value(value.name(),
                     conv.type().get(),
                     { data, ctx.intConstant(count, 64) },
                     loc,
                     Unpacked);
    }
#if 0
    case Reinterpret_Array_ToByte:
        [[fallthrough]];
    case Reinterpret_Array_FromByte:
        SC_UNIMPLEMENTED();
    case Reinterpret_Value_ToByteArray: {
        SC_UNIMPLEMENTED();
        //        auto data = value;
        //        auto* fromType = stripPtr(expr->type().get());
        //        auto* toType =
        //            cast<sema::ArrayType const*>(stripPtr(conv.type().get()));
        //        if (toType->isDynamic()) {
        //            valueMap.insertArraySize(conv.object(), fromType->size());
        //        }
        //        return data;
    }
    case Reinterpret_Value_FromByteArray: {
        SC_UNIMPLEMENTED();
        //        Value data = value;
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
    }
    case Reinterpret_Value: {
        SC_UNIMPLEMENTED();
        //        auto* result =
        //        add<ir::ConversionInst>(toRegister(value, conv),
        //                                               typeMap(conv.type()),
        //                                               ir::Conversion::Bitcast,
        //                                               "reinterpret");
        //        return Value(result, Register);
    }
#endif
    case SS_Trunc:
        [[fallthrough]];
    case SU_Trunc:
        [[fallthrough]];
    case US_Trunc:
        [[fallthrough]];
    case UU_Trunc:
        [[fallthrough]];
    case SS_Widen:
        [[fallthrough]];
    case SU_Widen:
        [[fallthrough]];
    case US_Widen:
        [[fallthrough]];
    case UU_Widen:
        [[fallthrough]];
    case Float_Trunc:
        [[fallthrough]];
    case Float_Widen:
        [[fallthrough]];
    case SignedToFloat:
        [[fallthrough]];
    case UnsignedToFloat:
        [[fallthrough]];
    case FloatToSigned:
        [[fallthrough]];
    case FloatToUnsigned: {
        auto name = utl::strcat(value.name(),
                                ".",
                                arithmeticConvName(conv.conversion()));
        auto* result =
            add<ir::ConversionInst>(to<Packed>(Register, value),
                                    typeMap.packed(conv.type().get()),
                                    mapArithmeticConv(conv.conversion()),
                                    name);
        return Value(name, conv.type().get(), { result }, Register, Packed);
    }
    }
    SC_UNIMPLEMENTED();
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

Value FuncGenContext::getValueImpl(ast::ConstructExpr const& expr) {
    SC_UNREACHABLE(); // This will be removed
    //    if (auto* type = dynArrayTypeCast(expr.type().get())) {
    //        SC_UNIMPLEMENTED();
    //        // return genConstructDynArray(expr, *type);
    //    }
    //    /// If we have a constructor we just call that
    //    if (!expr.isTrivial()) {
    //        SC_UNIMPLEMENTED();
    //        //        return genConstructNonTrivial(expr);
    //    }
    //    return genConstructTrivial(expr, *expr.type());
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

// static Value rpValue(std::string name, sema::ObjectType const* type,
// ir::Value* value) {
//     return Value(std::move(name), type, { value }, Register, Packed);
// }

Value FuncGenContext::getValueImpl(ast::TrivDefConstructExpr const& expr) {
    auto* type = expr.type().get();
    auto* irType = typeMap.packed(type);
    std::string name = "tmp";
    if (irType->size() <= PreferredMaxRegisterValueSize) {
        return Value(name,
                     type,
                     { makeZeroConstant(irType) },
                     Register,
                     Packed);
    }
    else {
        auto* addr = makeLocalVariable(irType, name);
        callMemset(addr, irType->size(), 0);
        return Value(name, type, { addr }, Memory, Packed);
    }
}

Value FuncGenContext::getValueImpl(ast::TrivCopyConstructExpr const& expr) {
    auto value = getValue(expr.arguments().front());
    auto* type = expr.type().get();
    std::string name = "tmp";
    if (type->size() <= PreferredMaxRegisterValueSize) {
        return Value(name, type, { toPackedRegister(value) }, Register, Packed);
    }
    else {
        auto* irType = typeMap.packed(type);
        auto* addr = makeLocalVariable(irType, name);
        auto* call = callMemcpy(addr, toPackedMemory(value), irType->size());
        addSourceLoc(call, expr);
        return Value(name, type, { addr }, Memory, Packed);
    }
}

Value FuncGenContext::getValueImpl(ast::TrivAggrConstructExpr const& expr) {
    auto* type = expr.type().get();
    auto* irType = cast<ir::StructType const*>(typeMap.packed(type));
    std::string name = "aggregate";
    if (irType->size() <= PreferredMaxRegisterValueSize) {
        utl::small_vector<ir::Value*> values;
        for (auto* arg: expr.arguments()) {
            for (auto* value: getValue<Unpacked>(Register, arg)) {
                values.push_back(value);
            }
        }
        auto* aggregate = buildStructure(irType, values, name);
        return Value(name, type, { aggregate }, Register, Packed);
    }
    else {
        ir::Value* mem = makeLocalVariable(irType, name);
        size_t index = 0;
        auto elemAddrName = [&] {
            return utl::strcat(name, ".elem.", index, ".addr");
        };
        for (auto* arg: expr.arguments()) {
            auto value = getValue(arg);
            auto members = to<Unpacked>(value.location(), value);
            auto* dest = add<ir::GetElementPointer>(ctx,
                                                    irType,
                                                    mem,
                                                    nullptr,
                                                    IndexArray{ index },
                                                    elemAddrName());
            if (value.isMemory()) {
                callMemcpy(dest,
                           members.front(),
                           irType->elementAt(index)->size());
            }
            else {
                add<ir::Store>(ctx, dest, members.front());
            }
            ++index;
            for (auto* member: members | drop(1)) {
                auto* dest = add<ir::GetElementPointer>(ctx,
                                                        irType,
                                                        mem,
                                                        nullptr,
                                                        IndexArray{ index },
                                                        elemAddrName());
                add<ir::Store>(ctx, dest, member);
                ++index;
            }
        }
        return Value(name, type, { mem }, Memory, Packed);
    }
}

Value FuncGenContext::getValueImpl(ast::NontrivConstructExpr const& expr) {
    auto* type = expr.constructedType();
    auto* irType = typeMap.packed(type);
    std::string name = "object";
    auto* mem = makeLocalVariable(irType, name);
    auto* irCtor = getFunction(expr.constructor());
    auto CC = getCC(expr.constructor());
    auto irArguments =
        unpackArguments(CC.arguments() | drop(1), getValues(expr.arguments()));
    irArguments.insert(irArguments.begin(), mem);
    add<ir::Call>(irCtor->returnType(), irCtor, irArguments);
    return Value(name, type, { mem }, Memory, Packed);
}

Value FuncGenContext::getValueImpl(ast::NontrivAggrConstructExpr const& expr) {
    auto* type = expr.constructedType();
    auto* irType = typeMap.packed(type);
    auto& metadata = typeMap.metaData(type);
    std::string name = "aggr";
    auto* mem = makeLocalVariable(irType, name);
    for (auto [index, arg]: expr.arguments() | enumerate) {
        size_t irIndex = metadata.members[index].beginIndex;
        auto elemName = utl::strcat(name, ".elem.", irIndex, ".addr");
        auto* destAddr = add<ir::GetElementPointer>(ctx,
                                                    irType,
                                                    mem,
                                                    nullptr,
                                                    IndexArray{ irIndex },
                                                    elemName);
        auto* val = getValue<Packed>(Memory, arg);
        SC_ASSERT(isa<ir::Alloca>(val), "Must be local memory to replace here");
        val->replaceAllUsesWith(destAddr);
    }
    return Value(name, type, { mem }, Memory, Packed);
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
//     generateCleanup(expr.dest()->object(), expr.dtor(), expr);
//     auto* function = getFunction(expr.ctor());
//     add<ir::Call>(function, ValueArray{ dest, source }, std::string{});
//     add<ir::Goto>(endBlock);
//     add(endBlock);
//     return Value();
// }

/// MARK: - General utilities

void FuncGenContext::generateCleanup(sema::Object const* object,
                                     sema::LifetimeOperation destroy,
                                     ast::ASTNode const&) {
    using enum sema::LifetimeOperation::Kind;
    switch (destroy.kind()) {
    case Trivial:
        /// These aren't actually generated but we can wouldn't have to do
        /// anything here
        break;
    case Nontrivial: {
        auto* function = getFunction(destroy.function());
        SC_ASSERT(ranges::distance(function->parameters()) == 1 &&
                      function->parameters().front().type() == ctx.ptrType(),
                  "Shall have single pointer argument");
        auto value = valueMap(object);
        SC_ASSERT(value.isMemory(), "Must be in memory");
        auto* addr = to<Packed>(Memory, value);
        add<ir::Call>(function, ValueArray{ addr }, std::string{});
        break;
    }
    case NontrivialInline:
        SC_UNIMPLEMENTED();
        break;
    case Deleted:
        SC_UNREACHABLE();
    }
}

void FuncGenContext::generateCleanups(sema::CleanupStack const& cleanupStack,
                                      ast::ASTNode const& sourceNode) {
    for (auto cleanup: cleanupStack) {
        generateCleanup(cleanup.object, cleanup.destroy, sourceNode);
    }
}

void FuncGenContext::addSourceLoc(ir::Instruction* inst,
                                  ast::ASTNode const& sourceNode) {
    if (!config.generateDebugSymbols) {
        return;
    }
    inst->setMetadata(sourceNode.sourceRange().begin());
}
