#include "IRGen/FunctionGeneration.h"

#include <range/v3/algorithm.hpp>
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
#include "Sema/Format.h"
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

/// Helper function to generate comments for constructor and destructor calls or
/// inline lifetime blocks. This makes the resulting IR easier to read
static std::string makeLifetimeComment(std::string_view kind,
                                       sema::Entity const* entity,
                                       sema::Type const* type) {
    return utl::strcat(kind, " for ", sema::format(entity ? entity : type));
}

/// \Overload
static std::string makeLifetimeComment(sema::Function const* ctor,
                                       sema::Entity const* entity) {
    using enum sema::SMFKind;
    auto kind = [&]() -> std::string_view {
        if (!ctor->smfKind()) {
            return "Other constructor";
        }
        switch (*ctor->smfKind()) {
        case DefaultConstructor:
            return "Default constructor";
        case CopyConstructor:
            return "Copy constructor";
        case MoveConstructor:
            return "Move constructor";
        case Destructor:
            return "Destructor";
        }
    }();
    return makeLifetimeComment(kind, entity,
                               cast<sema::Type const*>(ctor->parent()));
}

namespace {

enum class InlineLifetime { Array, UniquePtr };

} // namespace

static InlineLifetime getInlineLifetimeCase(sema::ObjectType const* type) {
    // clang-format off
    return SC_MATCH (*type) {
        [](sema::ArrayType const& ) { return InlineLifetime::Array; },
        [](sema::UniquePtrType const& ) { return InlineLifetime::UniquePtr; },
        [](sema::Type const& ) -> InlineLifetime { SC_UNREACHABLE(); },
    }; // clang-format on
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

    void generateSynthFunction();

    void generateSynthFunctionAs(sema::SMFKind kind);

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

    /// # Expressions
    SC_NODEBUG Value getValue(ast::Expression const* expr);

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
    Value getValueImpl(ast::MoveExpr const&);
    Value getValueImpl(ast::UniqueExpr const&);

    Value getValueImpl(ast::ValueCatConvExpr const&);
    Value getValueImpl(ast::MutConvExpr const&);
    Value getValueImpl(ast::ObjTypeConvExpr const&);

    Value getValueImpl(ast::TrivDefConstructExpr const&);
    Value getValueImpl(ast::TrivCopyConstructExpr const&);
    Value getValueImpl(ast::TrivAggrConstructExpr const&);
    Value getValueImpl(ast::NontrivConstructExpr const&);
    Value getValueImpl(ast::NontrivInlineConstructExpr const&);
    Value getValueImpl(ast::NontrivAggrConstructExpr const&);
    Value getValueImpl(ast::DynArrayConstructExpr const&);

    Value getValueImpl(ast::NontrivAssignExpr const&);

    /// # Lifetime specific utilities

    void generateCleanups(sema::CleanupStack const& cleanupStack);

    void generateLifetimeOperation(sema::SMFKind smfKind, Value dest,
                                   std::optional<Value> source);

    void inlineLifetime(sema::SMFKind kind, Value dest,
                        std::optional<Value> source);
    void inlineLifetimeImpl(sema::SMFKind, Value const&, std::optional<Value>,
                            sema::Type const&);
    void inlineLifetimeImpl(sema::SMFKind, Value const&, std::optional<Value>,
                            sema::ArrayType const&);
    void inlineLifetimeImpl(sema::SMFKind, Value const&, std::optional<Value>,
                            sema::UniquePtrType const&);

    /// # General utilities

    /// Generates \p numElements number of `GetElementPointer` instructions in
    /// the index range `[beginIndex, beginIndex + numElements)`
    utl::small_vector<ir::Value*> unpackStructMembers(
        ir::Value* address, ir::StructType const* parentType, size_t beginIndex,
        size_t numElements, std::string_view name);

    ///
    Value unpackStructMembersToValue(sema::ObjectType const* elemType,
                                     ir::Value* address,
                                     ir::StructType const* type,
                                     size_t beginIndex, size_t numElements,
                                     std::string_view name);
};

} // namespace

void irgen::generateAstFunction(Config config, FuncGenParameters params) {
    FuncGenContext funcCtx(config, params);
    funcCtx.generate(*params.semaFn.definition());
}

void irgen::generateSynthFunction(Config config, FuncGenParameters params) {
    FuncGenContext funcCtx(config, params);
    funcCtx.generateSynthFunction();
}

void FuncGenContext::generateSynthFunction() {
    auto kind = semaFn.smfKind().value();
    addNewBlock("entry");
    generateSynthFunctionAs(kind);
}

void FuncGenContext::generateSynthFunctionAs(sema::SMFKind kind) {
    auto* parentType = cast<sema::StructType const*>(semaFn.parent());
    auto* irParentType =
        cast<ir::StructType const*>(typeMap.packed(parentType));

    bool allChildrenTrivial =
        ranges::all_of(parentType->members() |
                           transform(cast<sema::ObjectType const*>),
                       [&](auto* type) {
        return type->lifetimeMetadata().operation(kind).isTrivial();
    });
    if (allChildrenTrivial) {
        auto* dest = &irFn.parameters().front();
        using enum sema::SMFKind;
        switch (kind) {
        case DefaultConstructor:
            callMemset(dest, parentType->size(), 0);
            return;
        case CopyConstructor:
            [[fallthrough]];
        case MoveConstructor:
            callMemcpy(dest, dest->next(), parentType->size());
            return;
        case Destructor:
            return;
        }
    }

    auto metadata = typeMap.metaData(parentType);
    auto* destAddr = &irFn.parameters().front();
    auto members =
        zip(parentType->memberVariables(), metadata.members) |
        transform([](auto p) { return std::pair(p.first, p.second); }) |
        ToSmallVector<>;
    using enum sema::SMFKind;
    if (kind == Destructor) {
        ranges::reverse(members);
    }
    for (auto [var, memberMd]: members) {
        size_t numElems = memberMd.fieldTypes.size();
        auto* memberType = cast<sema::ObjectType const*>(var->type());
        auto dest =
            unpackStructMembersToValue(memberType, destAddr, irParentType,
                                       memberMd.beginIndex, numElems,
                                       utl::strcat("dest.", var->name()));
        auto source = [&]() -> std::optional<Value> {
            if (kind != CopyConstructor && kind != MoveConstructor) {
                return std::nullopt;
            }
            auto* sourceAddr = irFn.parameters().front().next();
            return unpackStructMembersToValue(memberType, sourceAddr,
                                              irParentType, memberMd.beginIndex,
                                              numElems,
                                              utl::strcat("source.",
                                                          var->name()));
        }();
        generateLifetimeOperation(kind, dest, source);
    }
}

/// MARK: - Statements

void FuncGenContext::generate(ast::Statement const& stmt) {
    /// We don't emit dead code
    if (!stmt.reachable()) {
        return;
    }
    visit(stmt,
          [this](auto const& stmt) SC_NODEBUG { return generateImpl(stmt); });
}

void FuncGenContext::generateImpl(ast::ImportStatement const&) {
    /// No-op
}

void FuncGenContext::generateImpl(ast::CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    generateCleanups(cmpStmt.cleanupStack());
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
    addNewBlock("entry");
    /// If the function is a user defined special member function (constructor
    /// or destructor) we still generate the code of the non-user defined
    /// equivalent function and then append the user defined code. This way in a
    /// user defined destructor all member destructors get called and in a user
    /// defined constructor all member variables get initialized automatically
    if (auto kind = prologueAsSMF(semaFn)) {
        generateSynthFunctionAs(*kind);
        makeBlockCurrent(&irFn.back());
    }
    /// Here we add all parameters to our value map and store possibly mutable
    /// parameters (everything that's not a reference) to the stack
    auto CC = getCC(&semaFn);
    auto irParamItr = irFn.parameters().begin();
    if (CC.returnLocation() == Memory) {
        ++irParamItr;
    }
    for (auto [paramDecl, pc]:
         ranges::views::zip(def.parameters(), CC.arguments()))
    {
        generateParameter(paramDecl, pc, irParamItr);
    }
    generate(*def.body());
    if (auto kind = epilogueAsSMF(semaFn)) {
        generateSynthFunctionAs(*kind);
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
    ast::ParameterDeclaration const* paramDecl, PassingConvention pc,
    List<ir::Parameter>::iterator& irParamItr) {
    auto params = utl::small_vector<ir::Value*>(pc.numParams(), [&]() {
        return std::to_address(irParamItr++);
    });
    std::string name = isa<ast::ThisParameter>(paramDecl) ?
                           "this" :
                           std::string(paramDecl->name());
    auto* semaParam = paramDecl->object();
    auto* paramType = stripRef(semaParam->type());
    switch (pc.location(0)) {
    case Register: {
        /// Reference parameters are special: We don't store them to memory
        /// because they cannot be reassigned
        if (isa<sema::ReferenceType>(semaParam->type())) {
            auto atoms = params | enumerate | transform([&](auto p) {
                auto [index, param] = p;
                return Atom(param, index == 0 ? Memory : Register);
            }) | ToSmallVector<2>;
            valueMap.insert(semaParam,
                            Value(name, paramType, atoms,
                                  params.size() == 1 ? Packed : Unpacked));
        }
        else if (params.size() == 1) {
            auto* mem = storeToMemory(params.front(), name);
            valueMap.insert(semaParam,
                            Value::Packed(name, paramType, Atom(mem, Memory)));
        }
        else { // This is the dynamic array pointer case
            auto* packedVal = packValues(params, name);
            auto* mem = storeToMemory(packedVal, name);
            valueMap.insert(semaParam,
                            Value::Packed(name, paramType, Atom(mem, Memory)));
        }
        break;
    }

    case Memory:
        SC_ASSERT(params.size() == 1, "");
        valueMap.insert(semaParam, Value::Packed(name, paramType,
                                                 Atom(params.front(), Memory)));
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
        Atom address = toMemory(pack(getValue(varDecl.initExpr())).single());
        address->setName(utl::strcat(name, ".addr"));
        valueMap.insert(var,
                        Value::Packed(name, varDecl.initExpr()->type().get(),
                                      address));
    }
    generateCleanups(cleanupStack);
}

void FuncGenContext::generateImpl(
    ast::ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    generateCleanups(exprStatement.cleanupStack());
}

void FuncGenContext::generateImpl(ast::ReturnStatement const& stmt) {
    /// Simple case of `return;` in a void function
    if (!stmt.expression()) {
        generateCleanups(stmt.cleanupStack());
        add<ir::Return>(ctx.voidValue());
        return;
    }

    /// More complex `return <value>;` case
    auto retval = getValue(stmt.expression());
    auto CC = getCC(&semaFn);
    if (CC.returnLocation() == Register) {
        /// Pointers we keep in registers but references directly refer to the
        /// value in memory
        auto destLocation = CC.returnLocationAtCallsite();
        auto* value = to(destLocation, pack(retval).single(),
                         typeMap.packed(retval.type()), retval.name())
                          .get();
        generateCleanups(stmt.cleanupStack());
        add<ir::Return>(value);
        return;
    }
    else { // Return via memory
        auto* retvalDest = &irFn.parameters().front();
        retval = pack(retval);
        auto atom = retval.single();
        if (atom.isMemory()) {
            if (auto* allocaInst = dyncast<ir::Alloca*>(atom.get())) {
                allocaInst->replaceAllUsesWith(retvalDest);
            }
            else {
                SC_ASSERT(stmt.expression()->type()->hasTrivialLifetime(),
                          "We can only memcpy trivial lifetime types");
                callMemcpy(retvalDest, atom.get(), retval.type()->size());
            }
        }
        else {
            add<ir::Store>(retvalDest, atom.get());
        }
        generateCleanups(stmt.cleanupStack());
        add<ir::Return>(ctx.voidValue());
        return;
    }
}

void FuncGenContext::generateImpl(ast::IfStatement const& stmt) {
    auto* condition = toPackedRegister(getValue(stmt.condition()));
    generateCleanups(stmt.cleanupStack());
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
        auto* condition = toPackedRegister(getValue(loopStmt.condition()));
        generateCleanups(loopStmt.conditionDtorStack());
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
        generateCleanups(loopStmt.incrementDtorStack());
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
        auto* condition = toPackedRegister(getValue(loopStmt.condition()));
        generateCleanups(loopStmt.conditionDtorStack());
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
        auto* condition = toPackedRegister(getValue(loopStmt.condition()));
        generateCleanups(loopStmt.conditionDtorStack());
        add<ir::Branch>(condition, loopBody, loopEnd);

        /// End
        add(loopEnd);
        loopStack.pop();
        break;
    }
    }
    generateCleanups(loopStmt.cleanupStack());
}

void FuncGenContext::generateImpl(ast::JumpStatement const& jump) {
    generateCleanups(jump.cleanupStack());
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
        return Value::Packed("int.lit", lit.type().get(),
                             Atom::Register(
                                 ctx.intConstant(lit.value<APInt>())));
    case Boolean:
        return Value::Packed("bool.lit", lit.type().get(),
                             Atom::Register(
                                 ctx.intConstant(lit.value<APInt>())));
    case FloatingPoint:
        return Value::Packed("float.lit", lit.type().get(),
                             Atom::Register(
                                 ctx.floatConstant(lit.value<APFloat>())));
    case Null:
        return Value::Packed("null.lit", lit.type().get(),
                             Atom::Register(ctx.nullpointer()));
    case This:
        return valueMap(lit.object());

    case String: {
        auto const& text = lit.value<std::string>();
        auto name = nameFromSourceLoc("string", lit.sourceLocation());
        auto* data = ctx.stringLiteral(text);
        auto* global = mod.makeGlobalConstant(ctx, data, name);
        return Value::Unpacked(name, lit.type().get(),
                               { Atom::Memory(global),
                                 Atom::Register(
                                     ctx.intConstant(text.size(), 64)) });
    }
    case Char:
        return Value::Packed("char.lit", lit.type().get(),
                             Atom::Register(
                                 ctx.intConstant(lit.value<APInt>())));
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
        SC_ASSERT(operand[0].isMemory(),
                  "Operand must be in memory to be modified");
        ir::Value* opAddr = toPackedMemory(operand);
        ir::Type const* operandType =
            typeMap.packed(expr.operand()->type().get());
        ir::Value* operandValue = toPackedRegister(operand);
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
            return Value::Packed(operand.name(), expr.type().get(),
                                 Atom::Register(operandValue));
        }
    }

    case ast::UnaryOperator::Promotion:
        return getValue(expr.operand());

    case ast::UnaryOperator::Negation: {
        auto* operand = toPackedRegister(getValue(expr.operand()));
        auto operation =
            isa<sema::IntType>(expr.operand()->type().get()) ? Sub : FSub;
        auto* newValue =
            add<ir::ArithmeticInst>(ctx.arithmeticConstant(0, operand->type()),
                                    operand, operation, "negated");
        return Value::Packed("negated", expr.type().get(),
                             Atom::Register(newValue));
    }

    default:
        auto* operand = toPackedRegister(getValue(expr.operand()));
        auto* newValue =
            add<ir::UnaryArithmeticInst>(operand, mapUnaryOp(expr.operation()),
                                         "expr");
        return Value::Packed("expr", expr.type().get(),
                             Atom::Register(newValue));
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
        auto* lhs = toPackedRegister(getValue(expr.lhs()));
        auto* rhs = toPackedRegister(getValue(expr.rhs()));
        auto operation = mapArithmeticOp(type, expr.operation());
        auto* result = add<ir::ArithmeticInst>(lhs, rhs, operation, resName);
        return Value::Packed(resName, expr.type().get(),
                             Atom::Register(result));
    }

    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = toPackedRegister(getValue(expr.lhs()));
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
        auto* rhs = toPackedRegister(getValue(expr.rhs()));
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
        return Value::Packed("log.or", expr.type().get(),
                             Atom::Register(result));
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
        auto types = typeMap.unpacked(expr.lhs()->type().get());
        auto lhs = getValue(expr.lhs());
        auto rhs = getValue(expr.rhs());
        auto lhsElems = unpack(lhs);
        auto rhsElems = unpack(rhs);
        auto values = zip(lhsElems, rhsElems, types) |
                      transform([&](auto t) -> ir::Value* {
            auto [lhsElem, rhsElem, irType] = t;
            return add<ir::CompareInst>(
                toRegister(lhsElem, irType, lhs.name()).get(),
                toRegister(rhsElem, irType, rhs.name()).get(),
                mapCompareMode(type), mapCompareOp(expr.operation()), resName);
        }) | ToSmallVector<>;
        using enum ir::ArithmeticOperation;
        auto* combined =
            foldValues(expr.operation() == Equals ? And : Or, values, resName);
        return Value::Packed(resName, expr.type().get(),
                             Atom::Register(combined));
    }

    case Comma:
        (void)getValue(expr.lhs());  /// Evaluate and discard LHS
        return getValue(expr.rhs()); /// Evaluate and return RHS

    case Assignment: {
        auto lhs = getValue(expr.lhs());
        auto repr = lhs.representation();
        auto rhs = to(repr, getValue(expr.rhs()));
        auto irTypes = typeMap.map(repr, lhs.type());
        SC_ASSERT(lhs.size() == rhs.size(), "");
        SC_ASSERT(irTypes.size() == lhs.size(), "");
        for (auto [lhsAtom, rhsAtom, irType]: zip(lhs, rhs, irTypes)) {
            SC_ASSERT(lhsAtom.isMemory(), "");
            if (rhsAtom.isRegister()) {
                add<ir::Store>(lhsAtom.get(), rhsAtom.get());
            }
            else {
                callMemcpy(lhsAtom.get(), rhsAtom.get(), irType->size());
            }
        }
        return makeVoidValue("assignment.result");
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
        SC_ASSERT(lhs[0].isMemory(), "Must be in memory to assign");
        auto* rhs = toPackedRegister(getValue(expr.rhs()));
        auto operation = mapArithmeticAssignOp(type, expr.operation());
        auto* exprRes = add<ir::ArithmeticInst>(toPackedRegister(lhs), rhs,
                                                operation, resName);
        add<ir::Store>(toMemory(lhs[0]).get(), exprRes);
        return makeVoidValue("assignment.result");
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
    auto baseLoc = base[0].location();
    auto* baseVal = base[0].get();
    auto values = zip(iota(metaData.beginIndex), metaData.fieldTypes) |
                  transform([&](auto p) -> Atom {
        auto [index, type] = p;
        switch (baseLoc) {
        case Register: {
            auto* elem =
                add<ir::ExtractValue>(baseVal, IndexArray{ index }, name);
            return Atom::Register(elem);
        }
        case Memory: {
            ir::Value* value =
                add<ir::GetElementPointer>(typeMap.packed(base.type()), baseVal,
                                           ctx.intConstant(0, 64),
                                           IndexArray{ index }, name);
            return Atom::Memory(value);
        }
        }
    }) | ToSmallVector<2>;
    return Value::Unpacked(name, expr.type().get(), values);
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
            auto* size = toPackedRegister(
                getArraySize(expr.accessed()->type().get(), accessed));
            return add<ir::CompareInst>(size, ctx.intConstant(0, 64),
                                        ir::CompareMode::Signed,
                                        ir::CompareOperation::Equal, "empty");
        }();
        return Value::Packed("empty", expr.type().get(), Atom::Register(empty));
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
        switch (accessed[0].location()) {
        case Register: {
            SC_ASSERT(!arrayType->isDynamic(),
                      "Dynamic array cannot be in memory");
            size_t index = isFront ? 0 : arrayType->count() - 1;
            auto* elem = add<ir::ExtractValue>(toPackedRegister(accessed),
                                               IndexArray{ index }, name);
            return Value::Packed(name, expr.type().get(), Atom::Register(elem));
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
                return add<ir::ArithmeticInst>(toPackedRegister(count),
                                               ctx.intConstant(1, 64),
                                               ir::ArithmeticOperation::Sub,
                                               "back.index");
            }();
            auto* irElemType = typeMap.packed(expr.type().get());
            auto* elem = add<ir::GetElementPointer>(irElemType,
                                                    unpack(accessed)[0].get(),
                                                    index, IndexArray{},
                                                    utl::strcat(name, ".addr"));
            return Value::Packed(name, expr.type().get(), Atom::Memory(elem));
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
    auto name = utl::strcat(value.name(), ".deref");
    auto elems = value.elements() | ToSmallVector<2>;
    auto irTypes =
        typeMap.map(value.representation(), expr.referred()->type().get());
    elems[0] = Atom::Memory(toRegister(elems[0], irTypes[0], name).get());
    return Value(name, expr.type().get(), elems, value.representation());
}

Value FuncGenContext::getValueImpl(ast::AddressOfExpression const& expr) {
    auto value = unpack(getValue(expr.referred()));
    SC_ASSERT(value[0].isMemory(), "");
    auto elems = value.elements() | ToSmallVector<2>;
    elems[0] = Atom::Register(elems[0].get());
    return Value::Unpacked(utl::strcat(value.name(), ".addr"),
                           expr.type().get(), elems);
}

Value FuncGenContext::getValueImpl(ast::Conditional const& condExpr) {
    auto* cond = toPackedRegister(getValue(condExpr.condition()));
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
    auto repr = commonRepresentation(thenVal.representation(),
                                     elseVal.representation(), Unpacked);
    auto thenComRepr = withBlockCurrent(thenBlock, [&] {
        auto vals = to(repr, thenVal);
        return vals;
    });
    auto elseComRepr = withBlockCurrent(elseBlock, [&] {
        auto vals = to(repr, elseVal);
        return vals;
    });
    SC_ASSERT(thenComRepr.size() == elseComRepr.size(), "");
    auto commonLocations = zip(thenComRepr, elseComRepr) |
                           transform([](auto p) {
        auto [a, b] = p;
        return commonLocation(a.location(), b.location());
    }) | ToSmallVector<>;
    auto irTypes = typeMap.map(repr, condExpr.type().get());
    irTypes.resize(thenComRepr.size()); // Kinda hacky but whatever
    auto thenResolved = withBlockCurrent(thenBlock, [&] {
        auto vals = zip(commonLocations, thenComRepr, iota(size_t{ 0 })) |
                    transform([&](auto t) {
            auto [loc, atom, index] = t;
            return to(loc, atom, irTypes[index], thenVal.name()).get();
        }) | ToSmallVector<2>;
        add<ir::Goto>(endBlock);
        return vals;
    });
    auto elseResolved = withBlockCurrent(elseBlock, [&] {
        auto vals = zip(commonLocations, elseComRepr, iota(size_t{ 0 })) |
                    transform([&](auto t) {
            auto [loc, atom, index] = t;
            return to(loc, atom, irTypes[index], elseVal.name()).get();
        }) | ToSmallVector<2>;
        add<ir::Goto>(endBlock);
        return vals;
    });
    add(endBlock);
    /// If both values are rvalues in local memory they can share the memory
    {
        auto* mem = dyncast<ir::Alloca*>(thenResolved.front());
        auto loc = commonLocations.front();
        if (loc == Memory && condExpr.isRValue() && mem) {
            SC_ASSERT(isa<ir::Alloca>(elseResolved.front()), "");
            SC_ASSERT(elseResolved.size() == 1, "For now");
            elseResolved.front()->replaceAllUsesWith(thenResolved.front());
            return Value("cond", condExpr.type().get(),
                         { Atom(thenResolved.front(), loc) }, repr);
        }
    }
    /// Generate end block.
    auto atoms = zip(thenResolved, elseResolved, commonLocations) |
                 transform([&](auto t) -> Atom {
        auto& [thenVal, elseVal, loc] = t;
        auto* phi =
            add<ir::Phi>(std::array{ ir::PhiMapping{ thenBlock, thenVal },
                                     ir::PhiMapping{ elseBlock, elseVal } },
                         "cond");
        return Atom(phi, loc);
    }) | ToSmallVector<2>;
    return Value("cond", condExpr.type().get(), atoms, repr);
}

Value FuncGenContext::getValueImpl(ast::FunctionCall const& call) {
    ir::Callable* function = getFunction(call.function());
    auto name = "call.result";
    auto CC = getCC(call.function());
    auto irArguments =
        unpackArguments(CC.arguments(), getValues(call.arguments()));
    /// Allocate return value storage
    if (CC.returnLocation() == Memory) {
        auto* irReturnType = typeMap.packed(call.function()->returnType());
        irArguments.insert(irArguments.begin(),
                           makeLocalVariable(irReturnType,
                                             utl::strcat(name, ".addr")));
    }
    auto instName = [&]() -> std::string {
        if (isa<ir::VoidType>(function->returnType())) {
            return {};
        }
        return name;
    }();
    auto* callInst = add<ir::Call>(function, irArguments, instName);
    auto* retval = CC.returnLocation() == Memory ? irArguments.front() :
                                                   callInst;
    return Value::Packed(name, call.type().get(),
                         Atom(retval, CC.returnLocationAtCallsite()));
}

utl::small_vector<ir::Value*> FuncGenContext::unpackArguments(
    auto&& passingConventions, auto&& values) {
    utl::small_vector<ir::Value*> irArguments;
    for (auto [PC, value]: zip(passingConventions, values)) {
        auto locations = PC.locationsAtCallsite();
        auto unpacked = unpack(value);
        auto irTypes = typeMap.unpacked(value.type());
        irTypes.resize(unpacked.size());
        SC_ASSERT(PC.numParams() == unpacked.size(), "Argument count mismatch");
        for (auto [index, loc, atom]:
             zip(iota(size_t{ 0 }), locations, unpacked))
        {
            irArguments.push_back(
                to(loc, atom, irTypes[index], value.name()).get());
        }
    }
    return irArguments;
}

Value FuncGenContext::getValueImpl(ast::Subscript const& expr) {
    auto array = unpack(getValue(expr.callee()));
    auto* arrayAddr = toMemory(array[0]).get();
    auto* index = toPackedRegister(getValue(expr.argument(0)));
    auto* elemType = typeMap.packed(expr.type().get());
    auto* elemAddr = add<ir::GetElementPointer>(elemType, arrayAddr, index,
                                                IndexArray{}, "elem.addr");
    return Value::Packed("elem", expr.type().get(), Atom::Memory(elemAddr));
}

Value FuncGenContext::getValueImpl(ast::SubscriptSlice const& expr) {
    auto* arrayType = cast<sema::ArrayType const*>(expr.callee()->type().get());
    auto* elemType = typeMap.packed(arrayType->elementType());
    auto array = getValue(expr.callee());
    auto* arrayAddr = toMemory(array[0]).get();
    auto* lower = toPackedRegister(getValue(expr.lower()));
    auto* upper = toPackedRegister(getValue(expr.upper()));
    auto name = utl::strcat(array.name(), ".slice");
    auto* addr = add<ir::GetElementPointer>(elemType, arrayAddr, lower,
                                            IndexArray{},
                                            utl::strcat(name, ".addr"));
    auto* size = add<ir::ArithmeticInst>(upper, lower,
                                         ir::ArithmeticOperation::Sub,
                                         utl::strcat(name, ".count"));
    return Value::Unpacked(name, expr.type().get(),
                           { Atom::Memory(addr), Atom::Register(size) });
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
            add<ir::GetElementPointer>(elemType, dest,
                                       ctx.intConstant(index, 32), IndexArray{},
                                       utl::strcat("listexpr.elem.", index));
        auto value = pack(getValue(elem));
        auto* addr = toMemory(value[0]).get();
        addr->replaceAllUsesWith(elemAddr);
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
    return Value::Packed(name, list.type().get(), Atom::Memory(array));
}

Value FuncGenContext::getValueImpl(ast::MoveExpr const& expr) {
    auto value = getValue(expr.value());
    auto operation = expr.operation();
    /// No operation means the move expression has no effect
    if (!operation) {
        return value;
    }
    auto* irType = typeMap.packed(expr.type().get());
    auto name = utl::strcat("move.", value.name());
    using enum sema::LifetimeOperation::Kind;
    switch (operation->kind()) {
    case Trivial:
        return copyValue(value);

    case Nontrivial: {
        auto* mem = makeLocalVariable(irType, name);
        auto* semaCtor = operation->function();
        auto* irCtor = getFunction(semaCtor);
        auto CC = getCC(semaCtor);
        auto irArguments =
            unpackArguments(CC.arguments() | drop(1), single(value));
        irArguments.insert(irArguments.begin(), mem);
        auto* call = add<ir::Call>(irCtor->returnType(), irCtor, irArguments);
        call->setComment(makeLifetimeComment(semaCtor, expr.object()));
        return Value::Packed(name, expr.type().get(), Atom::Memory(mem));
    }

    case NontrivialInline:
        switch (getInlineLifetimeCase(expr.type().get())) {
        case InlineLifetime::Array:
            SC_UNIMPLEMENTED();
        case InlineLifetime::UniquePtr: {
            SC_ASSERT(
                value[0].isMemory() && value.isPacked(),
                "Must be packed memory because we set the old value to null here");
            auto* newVal = add<ir::Load>(value[0].get(), irType, name);
            add<ir::Store>(ctx, value[0].get(), makeZeroConstant(irType));
            return Value::Packed(name, expr.type().get(),
                                 toMemory(Atom::Register(newVal)));
        }
        default:
            SC_UNREACHABLE();
        }

    case Deleted:
        SC_UNREACHABLE();
    }
}

Value FuncGenContext::getValueImpl(ast::UniqueExpr const& expr) {
    auto* currentBBBefore = &currentBlock();
    auto backItr = std::prev(currentBlock().end());
    auto value = unpack(getValue(expr.value()));
    auto* addr = toMemory(value[0]).get();
    SC_ASSERT(
        isa<ir::Alloca>(addr) || isa<ir::NullPointerConstant>(addr),
        "We expect the argument to be constructed in local memory and we will rewrite it to heap allocation");
    /// We increment what used to be the back iterator here. Because we
    /// generated our expression in the meantime this now points to the first
    /// instruction generated by the expression
    auto insertBefore = std::next(backItr);
    ir::Callable* alloc = getBuiltin(svm::Builtin::alloc);
    sema::ObjectType const* baseType =
        cast<sema::UniquePtrType const&>(*expr.type()).base().get();
    std::string name = "unique";
    ir::Value* arrayCount = [&]() -> ir::Value* {
        if (isa<sema::ArrayType>(baseType)) {
            return toPackedRegister(getArraySize(baseType, value));
        }
        return nullptr;
    }();
    ir::Value* bytesize = [&]() -> ir::Value* {
        if (auto* arrayType = dyncast<sema::ArrayType const*>(baseType)) {
            size_t elemSize = arrayType->elementType()->size();
            if (auto* inst = dyncast<ir::Instruction*>(arrayCount)) {
                currentBBBefore = inst->parent();
                insertBefore = std::next(ir::BasicBlock::Iterator(inst));
            }
            return withBlockCurrent(currentBBBefore, insertBefore, [&] {
                return makeCountToByteSize(arrayCount, elemSize);
            });
        }
        else {
            return ctx.intConstant(baseType->size(), 64);
        }
    }();
    ValueArray args = { bytesize, ctx.intConstant(baseType->align(), 64) };
    return withBlockCurrent(currentBBBefore, insertBefore, [&] {
        ir::Value* arrayPtr =
            add<ir::Call>(alloc, args, utl::strcat(name, ".alloc"));
        ir::Value* ptr = add<ir::ExtractValue>(arrayPtr, IndexArray{ 0 },
                                               utl::strcat(name, ".pointer"));
        addr->replaceAllUsesWith(ptr);
        if (arrayCount) {
            return Value::Unpacked(name, expr.type().get(),
                                   { Atom::Register(ptr),
                                     Atom::Register(arrayCount) });
        }
        else {
            return Value::Packed(name, expr.type().get(),
                                 toMemory(Atom::Register(ptr)));
        }
    });
}

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
    case LValueToRValue:
        return copyValue(value);

    case MaterializeTemporary:
        auto elems = value.elements() | ToSmallVector<2>;
        elems[0] = toMemory(elems[0]);
        return Value(value.name(), value.type(), elems, value.representation());
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
        if (isDynArrayPointer(conv.type().get())) {
            return Value::Unpacked(value.name(), conv.type().get(),
                                   { Atom::Register(value[0].get()),
                                     Atom::Register(ctx.intConstant(0, 64)) });
        }
        else {
            return Value::Packed(value.name(), conv.type().get(),
                                 Atom::Register(value[0].get()));
        }
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
        value = unpack(value);
        SC_ASSERT(
            isa<sema::PointerType>(*expr->type()) || value[0].isMemory(),
            "Dynamic arrays cannot be in registers. For rvalues we should have a MaterializeTemporary conversion before this case");
        size_t count = getStaticArraySize(stripPtr(expr->type().get())).value();
        return Value::Unpacked(value.name(), conv.type().get(),
                               { value[0],
                                 Atom::Register(ctx.intConstant(count, 64)) });
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
        auto name = utl::strcat(value.name(), ".",
                                arithmeticConvName(conv.conversion()));
        auto* result =
            add<ir::ConversionInst>(toPackedRegister(value),
                                    typeMap.packed(conv.type().get()),
                                    mapArithmeticConv(conv.conversion()), name);
        return Value::Packed(name, conv.type().get(), Atom::Register(result));
    }
    }
    SC_UNIMPLEMENTED();
}

Value FuncGenContext::getValueImpl(ast::TrivDefConstructExpr const& expr) {
    auto* type = expr.type().get();
    auto* irType = typeMap.packed(type);
    std::string name = "tmp";
    if (irType->size() <= PreferredMaxRegisterValueSize) {
        return Value::Packed(name, type,
                             Atom::Register(makeZeroConstant(irType)));
    }
    else {
        auto* addr = makeLocalVariable(irType, name);
        callMemset(addr, irType->size(), 0);
        return Value::Packed(name, type, Atom::Memory(addr));
    }
}

Value FuncGenContext::getValueImpl(ast::TrivCopyConstructExpr const& expr) {
    auto value = getValue(expr.arguments().front());
    auto* type = expr.type().get();
    std::string name = "tmp";
    if (type->size() <= PreferredMaxRegisterValueSize) {
        return Value::Packed(name, type,
                             Atom::Register(toPackedRegister(value)));
    }
    else {
        auto* irType = typeMap.packed(type);
        auto* addr = makeLocalVariable(irType, name);
        callMemcpy(addr, toPackedMemory(value), irType->size());
        return Value::Packed(name, type, Atom::Memory(addr));
    }
}

Value FuncGenContext::getValueImpl(ast::TrivAggrConstructExpr const& expr) {
    auto* type = expr.type().get();
    auto* irType = cast<ir::StructType const*>(typeMap.packed(type));
    std::string name = "aggregate";
    if (irType->size() <= PreferredMaxRegisterValueSize) {
        utl::small_vector<ir::Value*> values;
        for (size_t index = 0; auto* arg: expr.arguments()) {
            auto value = unpack(getValue(arg));
            auto irTypes = typeMap.unpacked(arg->type().get());
            for (auto [atom, type]: zip(value, irTypes)) {
                values.push_back(
                    toRegister(atom, type, utl::strcat(name, ".elem.", index++))
                        .get());
            }
        }
        auto* aggregate = buildStructure(irType, values, name);
        return Value::Packed(name, type, Atom::Register(aggregate));
    }
    else {
        ir::Value* mem = makeLocalVariable(irType, name);
        size_t index = 0;
        auto elemAddrName = [&] {
            return utl::strcat(name, ".elem.", index, ".addr");
        };
        for (auto* arg: expr.arguments()) {
            auto value = unpack(getValue(arg));
            for (auto atom: value) {
                auto* dest = add<ir::GetElementPointer>(ctx, irType, mem,
                                                        nullptr,
                                                        IndexArray{ index },
                                                        elemAddrName());
                if (atom.isMemory()) {
                    callMemcpy(dest, atom.get(),
                               irType->elementAt(index)->size());
                }
                else {
                    add<ir::Store>(ctx, dest, atom.get());
                }
            }
            ++index;
        }
        return Value::Packed(name, type, Atom::Memory(mem));
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
    auto* call = add<ir::Call>(irCtor->returnType(), irCtor, irArguments);
    call->setComment(makeLifetimeComment(expr.constructor(), expr.object()));
    return Value::Packed(name, type, Atom::Memory(mem));
}

Value FuncGenContext::getValueImpl(
    ast::NontrivInlineConstructExpr const& expr) {
    auto* irType = typeMap.packed(expr.type().get());
    auto* mem = makeLocalVariable(irType, "value");
    auto dest = Value::Packed("value", expr.type().get(), Atom::Memory(mem));
    auto source = [&]() -> std::optional<Value> {
        switch (expr.arguments().size()) {
        case 0:
            return std::nullopt;
        case 1:
            return getValue(expr.argument(0));
        default:
            SC_UNREACHABLE();
        }
    }();
    inlineLifetime(expr.operation(), dest, source);
    return dest;
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
        auto* destAddr = add<ir::GetElementPointer>(ctx, irType, mem, nullptr,
                                                    IndexArray{ irIndex },
                                                    elemName);
        auto* val = toPackedMemory(getValue(arg));
        SC_ASSERT(isa<ir::Alloca>(val), "Must be local memory to replace here");
        val->replaceAllUsesWith(destAddr);
    }
    return Value::Packed(name, type, Atom::Memory(mem));
}

Value FuncGenContext::getValueImpl(ast::DynArrayConstructExpr const& expr) {
    auto* type = expr.constructedType();
    auto* irElemType = typeMap.packed(type->elementType());
    std::string name = "array";
    auto* count = toPackedRegister(getValue(expr.argument(0)));
    auto* arrayBegin = makeLocalArray(irElemType, count, name);
    /// Trivial default construction we can do with a memset, no need to
    /// generate a loop here
    if (isa<ast::TrivDefConstructExpr>(expr.elementConstruction())) {
        callMemset(arrayBegin, makeCountToByteSize(count, irElemType->size()),
                   0);
    }
    else {
        auto* arrayEnd = add<ir::GetElementPointer>(ctx, irElemType, arrayBegin,
                                                    count, IndexArray{},
                                                    utl::strcat(name, ".end"));
        auto loop = generateForLoopImpl(utl::strcat(name, ".constr"),
                                        arrayBegin, arrayEnd,
                                        [&](ir::Value* ind) {
            return add<ir::GetElementPointer>(ctx, irElemType, ind,
                                              ctx.intConstant(1, 64),
                                              IndexArray{},
                                              utl::strcat(name, ".ind"));
        });
        withBlockCurrent(loop.body, loop.insertPoint, [&] {
            auto* elem = toPackedMemory(getValue(expr.elementConstruction()));
            SC_ASSERT(isa<ir::Alloca>(elem), "Must be local");
            elem->replaceAllUsesWith(loop.induction);
        });
    }
    return Value::Unpacked(name, expr.type().get(),
                           { Atom::Memory(arrayBegin), Atom::Register(count) });
}

Value FuncGenContext::getValueImpl(ast::NontrivAssignExpr const& expr) {
    /// If the values are different, we  call the destructor of LHS and the copy
    /// or move constructor of LHS with RHS as argument. If the values are the
    /// same we do nothing
    auto dest = getValue(expr.dest());
    SC_ASSERT(dest[0].isMemory(), "Must be in memory to be assigned");
    auto source = to(dest.representation(), getValue(expr.source()));
    auto* assignBlock = newBlock("assign");
    auto* endBlock = newBlock("assign.end");
    if (expr.mustCheckForSelfAssignment()) {
        SC_ASSERT(source[0].isMemory(), "LValue must be in memory");
        auto* addrNeq = add<ir::CompareInst>(dest[0].get(), source[0].get(),
                                             ir::CompareMode::Unsigned,
                                             ir::CompareOperation::NotEqual,
                                             "assign.addr.neq");
        add<ir::Branch>(addrNeq, assignBlock, endBlock);
    }
    else {
        add<ir::Goto>(assignBlock);
    }
    add(assignBlock);
    generateLifetimeOperation(sema::SMFKind::Destructor, dest, std::nullopt);
    generateLifetimeOperation(expr.copyOperation(), dest, source);
    add<ir::Goto>(endBlock);
    add(endBlock);
    return makeVoidValue("assignment.result");
}

void FuncGenContext::generateCleanups(sema::CleanupStack const& cleanupStack) {
    for (auto cleanup: cleanupStack) {
        generateLifetimeOperation(sema::SMFKind::Destructor,
                                  valueMap(cleanup.object), std::nullopt);
    }
}

void FuncGenContext::generateLifetimeOperation(sema::SMFKind smfKind,
                                               Value dest,
                                               std::optional<Value> source) {
    if (dest.isPacked()) {
        dest = unpack(dest);
    }
    if (source) {
        if (source->isPacked()) {
            source = unpack(*source);
        }
        SC_EXPECT(dest.size() == source->size());
    }
    auto* type = dest.type();
    auto operation = type->lifetimeMetadata().operation(smfKind);
    auto irTypes = typeMap.unpacked(type);
    using enum sema::LifetimeOperation::Kind;
    using enum sema::SMFKind;
    switch (operation.kind()) {
    case Trivial:
        switch (smfKind) {
        case DefaultConstructor:
            SC_ASSERT(irTypes.size() == dest.size(), "");
            for (auto [addr, irType]: zip(dest, irTypes)) {
                if (irType->size() <= PreferredMaxRegisterValueSize) {
                    add<ir::Store>(ctx, addr.get(), makeZeroConstant(irType));
                }
                else {
                    callMemset(addr.get(), irType->size(), 0);
                }
            }
            break;
        case CopyConstructor:
            [[fallthrough]];
        case MoveConstructor:
            SC_EXPECT(source);
            SC_ASSERT(irTypes.size() == dest.size(), "");
            for (auto [dest, source, irType]: zip(dest, *source, irTypes)) {
                if (irType->size() <= PreferredMaxRegisterValueSize) {
                    auto* value = add<ir::Load>(source.get(), irType, "copy");
                    add<ir::Store>(ctx, dest.get(), value);
                }
                else {
                    callMemcpy(dest.get(), source.get(), irType->size());
                }
            }
            break;
        case Destructor:
            break;
        }
        break;
    case Nontrivial: {
        SC_ASSERT(dest.size() == 1, "");
        auto* irOperation = getFunction(operation.function());
        switch (smfKind) {
        case DefaultConstructor:
            add<ir::Call>(irOperation, ValueArray{ dest.single().get() });
            break;
        case CopyConstructor:
            [[fallthrough]];
        case MoveConstructor:
            SC_EXPECT(source);
            add<ir::Call>(irOperation, ValueArray{ dest.single().get(),
                                                   source->single().get() });
            break;
        case Destructor:
            add<ir::Call>(irOperation, ValueArray{ dest.single().get() });
            break;
        }
        break;
    }
    case NontrivialInline:
        inlineLifetime(smfKind, dest, source);
        break;
    case Deleted:
        SC_UNREACHABLE();
    }
}

void FuncGenContext::inlineLifetime(sema::SMFKind kind, Value dest,
                                    std::optional<Value> source) {
    visit(*dest.type(),
          [&](auto& type) { inlineLifetimeImpl(kind, dest, source, type); });
}

void FuncGenContext::inlineLifetimeImpl(sema::SMFKind, Value const&,
                                        std::optional<Value>,
                                        sema::Type const&) {
    SC_UNREACHABLE();
}

static std::string makeLifetimeLoopName(std::string_view base,
                                        sema::SMFKind kind) {
    using enum sema::SMFKind;
    switch (kind) {
    case DefaultConstructor:
        return utl::strcat(base, ".defcon");
    case CopyConstructor:
        return utl::strcat(base, ".copy");
    case MoveConstructor:
        return utl::strcat(base, ".move");
    case Destructor:
        return utl::strcat(base, ".destr");
    }
}

void FuncGenContext::inlineLifetimeImpl(sema::SMFKind kind, Value const& dest,
                                        std::optional<Value> source,
                                        sema::ArrayType const& type) {
    auto* elemType = type.elementType();
    auto* irElemType = typeMap.packed(elemType);
    auto* destSize = toPackedRegister(getArraySize(&type, dest));
    auto* destBegin = toMemory(unpack(dest)[0]).get();
    auto* destEnd =
        add<ir::GetElementPointer>(ctx, irElemType, destBegin, destSize,
                                   IndexArray{},
                                   utl::strcat(dest.name(), ".end"));
    destEnd->setComment(
        makeLifetimeComment("Destruction block", nullptr, &type));
    auto* pred = &currentBlock();
    auto loop = generateForLoopImpl(makeLifetimeLoopName(dest.name(), kind),
                                    destBegin, destEnd,
                                    [&](ir::Value* counter) {
        return add<ir::GetElementPointer>(ctx, irElemType, counter,
                                          ctx.intConstant(1, 64), IndexArray{},
                                          utl::strcat(dest.name(), ".ind"));
    });
    auto sourceElemValue = [&]() -> std::optional<Value> {
        using enum sema::SMFKind;
        if (kind != CopyConstructor && kind != MoveConstructor) {
            return std::nullopt;
        }
        auto* sourceBegin =
            withBlockCurrent(pred,
                             ir::BasicBlock::ConstIterator(pred->terminator()),
                             [&] {
            return toMemory(unpack(source.value())[0]).get();
        });
        auto* phi =
            new ir::Phi({ { pred, sourceBegin }, { loop.body, nullptr } },
                        utl::strcat(source->name(), ".counter"));
        auto* ind = withBlockCurrent(loop.body, loop.insertPoint, [&] {
            return add<ir::GetElementPointer>(ctx, irElemType, phi,
                                              ctx.intConstant(1, 64),
                                              IndexArray{},
                                              utl::strcat(source->name(),
                                                          ".ind"));
        });
        phi->setArgument(loop.body, ind);
        loop.body->insertPhi(phi);
        return Value::Packed("source.elem", type.elementType(),
                             Atom::Memory(phi));
    }();
    withBlockCurrent(loop.body, loop.insertPoint, [&] {
        generateLifetimeOperation(kind,
                                  Value::Packed("dest.elem", type.elementType(),
                                                Atom::Memory(loop.induction)),
                                  sourceElemValue);
    });
}

void FuncGenContext::inlineLifetimeImpl(sema::SMFKind kind, Value const& inDest,
                                        std::optional<Value> source,
                                        sema::UniquePtrType const& type) {
    auto* pointeeType = type.base().get();
    std::string name = "unique.ptr";

    using enum sema::SMFKind;
    switch (kind) {
    case DefaultConstructor: {
        auto irTypes = typeMap.map(inDest.representation(), &type);
        SC_ASSERT(irTypes.size() == inDest.size(), "");
        for (auto [dest, irType]: zip(inDest, irTypes)) {
            add<ir::Store>(ctx, dest.get(), makeZeroConstant(irType));
        }
        break;
    }
    case CopyConstructor:
        SC_UNREACHABLE();
    case MoveConstructor: {
        auto irTypes = typeMap.map(inDest.representation(), &type);
        SC_ASSERT(inDest.size() == source.value().size(), "");
        SC_ASSERT(irTypes.size() == inDest.size(), "");
        for (auto [dest, source, irType]: zip(inDest, *source, irTypes)) {
            auto* sourceVal = toRegister(source, irType, "copy").get();
            SC_ASSERT(
                source.isMemory(),
                "We must set this to zero otherwise it may be deleted again");
            add<ir::Store>(ctx, source.get(), makeZeroConstant(irType));
            add<ir::Store>(ctx, dest.get(), sourceVal);
        }
        break;
    }
    case Destructor: {
        auto dest = unpack(inDest);
        auto* data =
            toRegister(dest[0], ctx.ptrType(), utl::strcat(name, ".data"))
                .get();
        auto* deleteBlock = newBlock(utl::strcat(name, ".delete"));
        auto* endBlock = newBlock(utl::strcat(name, ".end"));
        auto* cmp = add<ir::CompareInst>(ctx, data, ctx.nullpointer(),
                                         ir::CompareMode::Unsigned,
                                         ir::CompareOperation::NotEqual,
                                         utl::strcat(name, ".engaged"));
        auto* branch = add<ir::Branch>(ctx, cmp, deleteBlock, endBlock);
        branch->setComment(
            makeLifetimeComment("Destruction block", nullptr, &type));

        add(deleteBlock);
        generateLifetimeOperation(sema::SMFKind::Destructor,
                                  Value::Unpacked("pointee", pointeeType,
                                                  dest.elements()),
                                  std::nullopt);
        auto [bytesize, align] = [&]() -> ValueArray<2> {
            if (dest.size() == 1) {
                return { ctx.intConstant(type.size(), 64),
                         ctx.intConstant(type.align(), 64) };
            }
            auto* elemType =
                cast<sema::ArrayType const*>(pointeeType)->elementType();
            auto* count = toRegister(dest[1], ctx.intType(64),
                                     utl::strcat(name, ".count"))
                              .get();
            return { makeCountToByteSize(count, elemType->size()),
                     ctx.intConstant(elemType->align(), 64) };
        }();
        auto* dealloc = getBuiltin(svm::Builtin::dealloc);
        add<ir::Call>(dealloc, ValueArray{ data, bytesize, align });
        add<ir::Goto>(ctx, endBlock);

        add(endBlock);
        break;
    }
    }
}

utl::small_vector<ir::Value*> FuncGenContext::unpackStructMembers(
    ir::Value* address, ir::StructType const* parentType, size_t beginIndex,
    size_t numElements, std::string_view name) {
    return iota(beginIndex, beginIndex + numElements) |
           transform([&](size_t index) -> ir::Value* {
        return add<ir::GetElementPointer>(parentType, address, nullptr,
                                          IndexArray{ index },
                                          std::string(name));
    }) | ToSmallVector<>;
}

Value FuncGenContext::unpackStructMembersToValue(
    sema::ObjectType const* elemType, ir::Value* address,
    ir::StructType const* parentType, size_t beginIndex, size_t numElements,
    std::string_view name) {
    auto addresses =
        unpackStructMembers(address, parentType, beginIndex, numElements, name);
    auto atoms = addresses | transform(Atom::Memory) | ToSmallVector<>;
    return Value::Unpacked(std::string(name), elemType, atoms);
}
