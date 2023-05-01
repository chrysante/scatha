#include "AST/LowerToIR.h"

#include <sstream>
#include <string>

#include <range/v3/view.hpp>
#include <utl/stack.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
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

namespace {

struct Loop {
    ir::BasicBlock* header = nullptr;
    ir::BasicBlock* body   = nullptr;
    ir::BasicBlock* inc    = nullptr;
    ir::BasicBlock* end    = nullptr;
};

struct CodeGenContext {
    explicit CodeGenContext(ir::Context& irCtx,
                            ir::Module& mod,
                            sema::SymbolTable const& symTable):
        irCtx(irCtx), mod(mod), symTable(symTable) {}

    void generate(AbstractSyntaxTree const& node);

    void generateImpl(AbstractSyntaxTree const& node) { SC_UNREACHABLE(); }
    void generateImpl(TranslationUnit const&);
    void generateImpl(CompoundStatement const&);
    void generateImpl(FunctionDefinition const&);
    void generateImpl(StructDefinition const&);
    void generateImpl(VariableDeclaration const&);
    void generateImpl(ParameterDeclaration const&);
    void generateImpl(ExpressionStatement const&);
    void generateImpl(EmptyStatement const&);
    void generateImpl(ReturnStatement const&);
    void generateImpl(IfStatement const&);
    void generateImpl(LoopStatement const&);
    void generateImpl(JumpStatement const&);

    /// **Always** dereferences implicit references
    /// Only explicit references are returned as pointers
    ir::Value* getValue(Expression const& expr);

    /// Return the logical address, i.e. if expression is an implicit reference,
    /// it returns the address it refers to, if it is an l-value object then it
    /// returns the address of that
    ir::Value* getAddress(Expression const& node);

    /// Returns the address of a reference to reassign it
    ir::Value* getReferenceAddress(Expression const& node);

    /// `getValueImpl()` returns references as pointers. Thus it is the
    /// responsibility of `getValue()` to dereference those pointers
    ir::Value* getValueImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getValueImpl(Expression const& expr) { SC_UNREACHABLE(); }
    ir::Value* getValueImpl(Identifier const&);
    ir::Value* getValueImpl(Literal const&);
    ir::Value* getValueImpl(UnaryPrefixExpression const&);
    ir::Value* getValueImpl(BinaryExpression const&);
    ir::Value* getValueImpl(MemberAccess const&);
    ir::Value* getValueImpl(ReferenceExpression const&);
    ir::Value* getValueImpl(UniqueExpression const&);
    ir::Value* getValueImpl(Conditional const&);
    ir::Value* getValueImpl(FunctionCall const&);
    ir::Value* genCallImpl(FunctionCall const&);
    ir::Value* getValueImpl(Subscript const&);
    ir::Value* getValueImpl(ImplicitConversion const&);
    ir::Value* getValueImpl(ListExpression const&);

    /// Store a temporary value to memory
    /// \Returns the address
    ir::Value* storeTmpToMemory(ir::Value* value,
                                ir::Type const* type = nullptr,
                                std::string name     = {});

    /// `getAddressImpl()` always returns the addresses of reference variables.
    /// It is the responsibility of `getAddress()` to dereference the address to
    /// get the actual address Only when called by `getReferenceAddress()` is
    /// the address if the reference returned
    ir::Value* getAddressImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getAddressImpl(Expression const& expr);
    ir::Value* getAddressImpl(Literal const& lit);
    ir::Value* getAddressImpl(Identifier const&);
    ir::Value* getAddressImpl(MemberAccess const&);
    ir::Value* getAddressImpl(FunctionCall const&);
    ir::Value* getAddressImpl(Subscript const&);
    ir::Value* getAddressImpl(ReferenceExpression const&);
    ir::Value* getAddressImpl(ImplicitConversion const&);
    ir::Value* getAddressImpl(ListExpression const&);

    ir::Value* loadAddress(ir::Value* address,
                           ir::Type const* type,
                           std::string_view name);

    ir::Value* makeArrayView(ir::Value* address, ir::Value* count);

    void declareTypes();
    void declareFunctions();

    /// To make sure all `alloca` instructions are in the entry basic block
    void addAlloca(ir::Alloca* allc) {
        allocaInsertItr =
            currentFunction->entry().insert(allocaInsertItr, allc)->next();
    }

    ir::BasicBlock* currentBB() { return _currentBB; }
    void setCurrentBB(ir::BasicBlock*);

    void memorizeVariableAddress(sema::Entity const* variable, ir::Value*);

    void memorizeVariableValue(sema::Entity const* variable, ir::Value*);

    ir::Type const* mapType(sema::Type const* semaType);
    utl::small_vector<ir::Type const*> mapParameterTypes(
        std::span<sema::QualType const* const> types);
    utl::small_vector<ir::Value*> mapArguments(ranges::range auto&& args);
    void generateParameter(ir::BasicBlock* entry,
                           List<ir::Parameter>::iterator& paramItr,
                           ParameterDeclaration const* paramDecl);
    ir::UnaryArithmeticOperation mapUnaryArithmeticOp(
        ast::UnaryPrefixOperator) const;
    ir::CompareMode mapCompareMode(sema::StructureType const*) const;
    ir::CompareOperation mapCompareOp(ast::BinaryOperator) const;
    ir::ArithmeticOperation mapArithmeticOp(sema::StructureType const* type,
                                            ast::BinaryOperator) const;
    ir::ArithmeticOperation mapArithmeticAssignOp(
        sema::StructureType const* type, ast::BinaryOperator) const;

    ir::Context& irCtx;
    ir::Module& mod;
    sema::SymbolTable const& symTable;
    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* _currentBB    = nullptr;
    utl::hashmap<sema::Entity const*, ir::Value*> variableAddressMap;
    utl::hashmap<sema::Entity const*, ir::Value*> variableValueMap;
    utl::hashmap<sema::Function const*, ir::Callable*> functionMap;
    utl::hashmap<sema::Type const*, ir::Type const*> typeMap;
    ir::Type const* arrayViewType = nullptr;
    ir::Instruction* allocaInsertItr;
    Loop currentLoop;
};

} // namespace

ir::Module ast::lowerToIR(AbstractSyntaxTree const& ast,
                          sema::SymbolTable const& symbolTable,
                          ir::Context& irCtx) {
    ir::Module mod;
    CodeGenContext ctx(irCtx, mod, symbolTable);
    ctx.declareTypes();
    ctx.declareFunctions();
    ctx.generate(ast);
    ir::setupInvariants(irCtx, mod);
    ir::assertInvariants(irCtx, mod);
    return mod;
}

void CodeGenContext::generate(AbstractSyntaxTree const& node) {
    visit(node, [this](auto const& node) { return generateImpl(node); });
}

void CodeGenContext::generateImpl(TranslationUnit const& tu) {
    for (auto* decl: tu.declarations()) {
        generate(*decl);
    }
}

void CodeGenContext::generateImpl(CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
}

void CodeGenContext::generateImpl(FunctionDefinition const& def) {
    auto paramTypes = def.parameters() |
                      ranges::views::transform(
                          [&](auto* param) { return mapType(param->type()); }) |
                      ranges::to<utl::small_vector<ir::Type const*>>;
    auto* fn = cast<ir::Function*>(functionMap.find(def.function())->second);
    currentFunction = fn;
    auto* entry     = new ir::BasicBlock(irCtx, "entry");
    fn->pushBack(entry);
    setCurrentBB(entry);
    allocaInsertItr = entry->begin().to_address();
    auto paramItr   = fn->parameters().begin();
    for (auto* paramDecl: def.parameters()) {
        generateParameter(entry, paramItr, paramDecl);
    }
    generate(*def.body());
    setCurrentBB(nullptr);
    currentFunction = nullptr;
}

void CodeGenContext::generateImpl(StructDefinition const& def) {
    for (auto* statement:
         def.body()->statements() | ranges::views::filter([](auto* statement) {
             return isa<FunctionDefinition>(statement);
         }))
    {
        generate(*statement);
    }
}

void CodeGenContext::generateImpl(VariableDeclaration const& varDecl) {
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(varDecl.type()->base()))
    {
        /// Array case
        auto* address = [&]() -> ir::Value* {
            if (varDecl.initExpression()) {
                if (varDecl.initExpression()->isRValue()) {
                    return getAddress(*varDecl.initExpression());
                }
                else {
                    // TODO: Copy the array
                    SC_DEBUGFAIL();
                }
            }
            auto* array = new ir::Alloca(irCtx,
                                         mapType(arrayType->elementType()),
                                         utl::strcat(varDecl.name(), ".addr"));
            addAlloca(array);
            return array;
        }();
        /// Stupid hack: We make an array view object with our address and our
        /// size and store that to memory. Then this array works just like an
        /// array view
        auto* count     = irCtx.integralConstant(APInt(arrayType->count(), 64));
        auto* arrayView = makeArrayView(address, count);
        auto* viewAddr  = storeTmpToMemory(arrayView);
        memorizeVariableAddress(varDecl.variable(), viewAddr);
        return;
    }
    /// Non-array case
    ir::Value* initValue = varDecl.initExpression() ?
                               getValue(*varDecl.initExpression()) :
                               nullptr;
    variableAddressMap[varDecl.variable()] =
        storeTmpToMemory(initValue,
                         mapType(varDecl.type()),
                         std::string(varDecl.name()));
}

void CodeGenContext::generateImpl(ParameterDeclaration const&) {
    SC_UNREACHABLE("Handled by generate(FunctionDefinition)");
}

void CodeGenContext::generateImpl(ExpressionStatement const& exprStatement) {
    (void)getValue(*exprStatement.expression());
}

void CodeGenContext::generateImpl(EmptyStatement const& empty) {
    /// Nothing to do here.
}

void CodeGenContext::generateImpl(ReturnStatement const& retDecl) {
    auto* returnValue = retDecl.expression() ? getValue(*retDecl.expression()) :
                                               irCtx.voidValue();
    currentBB()->pushBack(new ir::Return(irCtx, returnValue));
}

void CodeGenContext::generateImpl(IfStatement const& ifStatement) {
    auto* condition = getValue(*ifStatement.condition());
    auto* thenBlock = new ir::BasicBlock(irCtx, "if.then");
    auto* elseBlock = ifStatement.elseBlock() ?
                          new ir::BasicBlock(irCtx, "if.else") :
                          nullptr;
    auto* endBlock  = new ir::BasicBlock(irCtx, "if.end");
    auto* branch    = new ir::Branch(irCtx,
                                  condition,
                                  thenBlock,
                                  elseBlock ? elseBlock : endBlock);
    currentBB()->pushBack(branch);
    auto addBlock = [&](ir::BasicBlock* bb, ast::Statement const& block) {
        currentFunction->pushBack(bb);
        setCurrentBB(bb);
        generate(block);
        auto* gotoEnd = new ir::Goto(irCtx, endBlock);
        currentBB()->pushBack(gotoEnd);
    };
    addBlock(thenBlock, *ifStatement.thenBlock());
    if (ifStatement.elseBlock()) {
        addBlock(elseBlock, *ifStatement.elseBlock());
    }
    currentFunction->pushBack(endBlock);
    setCurrentBB(endBlock);
}

void CodeGenContext::generateImpl(LoopStatement const& loopStmt) {
    switch (loopStmt.kind()) {
    case ast::LoopKind::For: {
        auto* loopHeader = new ir::BasicBlock(irCtx, "loop.header");
        auto* loopBody   = new ir::BasicBlock(irCtx, "loop.body");
        auto* loopInc    = new ir::BasicBlock(irCtx, "loop.inc");
        auto* loopEnd    = new ir::BasicBlock(irCtx, "loop.end");
        generate(*loopStmt.varDecl());
        auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
        currentBB()->pushBack(gotoLoopHeader);
        /// Header
        currentFunction->pushBack(loopHeader);
        setCurrentBB(loopHeader);
        auto* condition = getValue(*loopStmt.condition());
        auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
        currentBB()->pushBack(branch);
        currentLoop = { .header = loopHeader,
                        .body   = loopBody,
                        .inc    = loopInc,
                        .end    = loopEnd };
        /// Body
        currentFunction->pushBack(loopBody);
        setCurrentBB(loopBody);
        generate(*loopStmt.block());
        auto* gotoLoopInc = new ir::Goto(irCtx, loopInc);
        /// Inc
        currentFunction->pushBack(loopInc);
        currentBB()->pushBack(gotoLoopInc);
        setCurrentBB(loopInc);
        getValue(*loopStmt.increment());
        auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
        currentBB()->pushBack(gotoLoopHeader2);
        /// End
        currentFunction->pushBack(loopEnd);
        setCurrentBB(loopEnd);
        currentLoop = {};
        break;
    }
    case ast::LoopKind::While: {
        auto* loopHeader     = new ir::BasicBlock(irCtx, "loop.header");
        auto* loopBody       = new ir::BasicBlock(irCtx, "loop.body");
        auto* loopEnd        = new ir::BasicBlock(irCtx, "loop.end");
        auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
        currentBB()->pushBack(gotoLoopHeader);
        /// Header
        currentFunction->pushBack(loopHeader);
        setCurrentBB(loopHeader);
        auto* condition = getValue(*loopStmt.condition());
        auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
        currentBB()->pushBack(branch);
        currentLoop = { .header = loopHeader,
                        .body   = loopBody,
                        .end    = loopEnd };
        /// Body
        currentFunction->pushBack(loopBody);
        setCurrentBB(loopBody);
        generate(*loopStmt.block());
        auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
        currentBB()->pushBack(gotoLoopHeader2);
        /// End
        currentFunction->pushBack(loopEnd);
        setCurrentBB(loopEnd);
        currentLoop = {};
        break;
    }
    case ast::LoopKind::DoWhile: {
        auto* loopBody     = new ir::BasicBlock(irCtx, "loop.body");
        auto* loopFooter   = new ir::BasicBlock(irCtx, "loop.footer");
        auto* loopEnd      = new ir::BasicBlock(irCtx, "loop.end");
        auto* gotoLoopBody = new ir::Goto(irCtx, loopBody);
        currentBB()->pushBack(gotoLoopBody);
        currentLoop = { .header = loopFooter,
                        .body   = loopBody,
                        .end    = loopEnd };
        /// Body
        currentFunction->pushBack(loopBody);
        setCurrentBB(loopBody);
        generate(*loopStmt.block());
        auto* gotoLoopFooter = new ir::Goto(irCtx, loopFooter);
        currentBB()->pushBack(gotoLoopFooter);
        /// Footer
        currentFunction->pushBack(loopFooter);
        setCurrentBB(loopFooter);
        auto* condition = getValue(*loopStmt.condition());
        auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
        currentBB()->pushBack(branch);
        /// End
        currentFunction->pushBack(loopEnd);
        setCurrentBB(loopEnd);
        currentLoop = {};
        break;
    }
    }
}

void CodeGenContext::generateImpl(JumpStatement const& jump) {
    auto* dest = [&] {
        switch (jump.kind()) {
        case JumpStatement::Break:
            return currentLoop.end;
        case JumpStatement::Continue:
            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
        }
    }();
    auto* gotoEnd = new ir::Goto(irCtx, dest);
    currentBB()->pushBack(gotoEnd);
}

ir::Value* CodeGenContext::getValue(Expression const& expr) {
    return visit(expr, [this](auto const& expr) { return getValueImpl(expr); });
}

ir::Value* CodeGenContext::getAddress(Expression const& expr) {
    SC_ASSERT(expr.isLValue() || expr.type()->isReference() || isArray(&expr),
              "Only l-values and references can have addresses (and array "
              "types for now)");
    auto* address =
        visit(expr, [this](auto const& expr) { return getAddressImpl(expr); });
    if (expr.isLValue() && (expr.type()->isReference() || isArray(&expr))) {
        /// L-value references have addresses themselves. They need to be
        /// dereferenced
        auto* addressType =
            isArray(&expr) ? arrayViewType : irCtx.pointerType();
        return loadAddress(address, addressType, "addr");
    }
    return address;
}

ir::Value* CodeGenContext::getReferenceAddress(Expression const& expr) {
    SC_ASSERT(expr.isLValue() && (expr.type()->isReference() || isArray(&expr)),
              "Only references that are l-values have addresses themselves");
    return visit(expr,
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* CodeGenContext::getValueImpl(Identifier const& id) {
    auto* address = getAddress(id);
    return loadAddress(address, mapType(id.type()->base()), id.value());
}

ir::Value* CodeGenContext::getValueImpl(Literal const& lit) {
    switch (lit.kind()) {
    case LiteralKind::Integer:
        return irCtx.integralConstant(lit.value<LiteralKind::Integer>());
    case LiteralKind::Boolean:
        return irCtx.integralConstant(lit.value<LiteralKind::Boolean>());
    case LiteralKind::FloatingPoint:
        return irCtx.floatConstant(lit.value<LiteralKind::FloatingPoint>(), 64);
    case LiteralKind::This: {
        auto* address = getAddressImpl(lit);
        return loadAddress(address, mapType(lit.type()->base()), "__this");
    }
    case LiteralKind::String:
        SC_DEBUGFAIL();
    }
}

ir::Value* CodeGenContext::getValueImpl(UnaryPrefixExpression const& expr) {
    switch (expr.operation()) {
    case ast::UnaryPrefixOperator::Increment:
        [[fallthrough]];
    case ast::UnaryPrefixOperator::Decrement: {
        ir::Value* const operandAddress = getAddress(*expr.operand());
        SC_ASSERT(isa<ir::PointerType>(operandAddress->type()),
                  "We need a pointer to store to");
        ir::Type const* arithmeticType =
            mapType(expr.operand()->type()->base());
        ir::Value* operandValue =
            loadAddress(operandAddress,
                        arithmeticType,
                        utl::strcat(expr.operation(), ".value"));
        auto const operation =
            expr.operation() == ast::UnaryPrefixOperator::Increment ?
                ir::ArithmeticOperation::Add :
                ir::ArithmeticOperation::Sub;
        auto* newValue =
            new ir::ArithmeticInst(operandValue,
                                   irCtx.integralConstant(APInt(1, 64)),
                                   operation,
                                   utl::strcat(expr.operation(), ".result"));
        currentBB()->pushBack(newValue);
        auto* store = new ir::Store(irCtx, operandAddress, newValue);
        currentBB()->pushBack(store);
        return newValue;
    }
    case ast::UnaryPrefixOperator::Promotion:
        return getValue(*expr.operand());
    case ast::UnaryPrefixOperator::Negation: {
        ir::Value* const operand = getValue(*expr.operand());
        SC_ASSERT(isa<ir::ArithmeticType>(operand->type()),
                  "The other unary operators operate on arithmetic values");
        auto* zero     = irCtx.arithmeticConstant(0, operand->type());
        auto operation = isa<ir::IntegralType>(operand->type()) ?
                             ir::ArithmeticOperation::Sub :
                             ir::ArithmeticOperation::FSub;
        auto* inst =
            new ir::ArithmeticInst(zero, operand, operation, "negated");
        currentBB()->pushBack(inst);
        return inst;
    }
    default: {
        ir::Value* const operand = getValue(*expr.operand());
        SC_ASSERT(isa<ir::ArithmeticType>(operand->type()),
                  "The other unary operators operate on arithmetic values");
        auto* inst =
            new ir::UnaryArithmeticInst(irCtx,
                                        operand,
                                        mapUnaryArithmeticOp(expr.operation()),
                                        std::string("expr.result"));
        currentBB()->pushBack(inst);
        return inst;
    }
    }
}

ir::Value* CodeGenContext::getValueImpl(BinaryExpression const& expr) {
    auto* structType =
        cast<sema::StructureType const*>(expr.lhs()->type()->base());

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
        ir::Value* const lhs = getValue(*expr.lhs());
        ir::Value* const rhs = getValue(*expr.rhs());
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
        auto* arithInst =
            new ir::ArithmeticInst(lhs, rhs, operation, "expr.result");
        currentBB()->pushBack(arithInst);
        return arithInst;
    }
    case LogicalAnd:
        [[fallthrough]];
    case LogicalOr: {
        ir::Value* const lhs = getValue(*expr.lhs());
        SC_ASSERT(isIntType(1, lhs->type()), "Need i1 for logical operation");
        auto* startBlock = currentBB();
        auto* rhsBlock   = new ir::BasicBlock(irCtx, "log.rhs");
        auto* endBlock   = new ir::BasicBlock(irCtx, "log.end");
        currentBB()->pushBack(
            expr.operation() == LogicalAnd ?
                new ir::Branch(irCtx, lhs, rhsBlock, endBlock) :
                new ir::Branch(irCtx, lhs, endBlock, rhsBlock));
        currentFunction->pushBack(rhsBlock);
        setCurrentBB(rhsBlock);
        auto* rhs = getValue(*expr.rhs());
        SC_ASSERT(isIntType(1, rhs->type()), "Need i1 for logical operation");
        currentBB()->pushBack(new ir::Goto(irCtx, endBlock));
        currentFunction->pushBack(endBlock);
        setCurrentBB(endBlock);
        auto* result =
            expr.operation() == LogicalAnd ?
                new ir::Phi({ { startBlock, irCtx.integralConstant(0, 1) },
                              { rhsBlock, rhs } },
                            "log.and.value") :
                new ir::Phi({ { startBlock, irCtx.integralConstant(1, 1) },
                              { rhsBlock, rhs } },
                            "log.or.value");
        currentBB()->pushBack(result);
        return result;
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
        auto* lhs = getValue(*expr.lhs());
        auto* rhs = getValue(*expr.rhs());
        SC_ASSERT(lhs->type() == rhs->type(), "Need same type for comparison");
        auto* cmpInst = new ir::CompareInst(irCtx,
                                            lhs,
                                            rhs,
                                            mapCompareMode(structType),
                                            mapCompareOp(expr.operation()),
                                            "cmp.result");
        currentBB()->pushBack(cmpInst);
        return cmpInst;
    }
    case Comma: {
        getValue(*expr.lhs());
        return getValue(*expr.rhs());
    }
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
        auto* rhs = getValue(*expr.rhs());
        /// If user assign an implicit reference, sema should have inserted an
        /// `ImplicitConversion` node that gives us a non-reference type here
        if (expr.rhs()->type()->isReference()) {
            /// Here we want to reassign the reference
            SC_ASSERT(isa<ir::PointerType>(rhs->type()),
                      "Explicit reference must resolve to pointer");
            SC_ASSERT(expr.operation() == Assignment,
                      "Can't do arithmetic with addresses");
            auto* lhs   = getReferenceAddress(*expr.lhs());
            auto* store = new ir::Store(irCtx, lhs, rhs);
            currentBB()->pushBack(store);
            return store;
        }
        SC_ASSERT(!isa<ir::PointerType>(rhs->type()),
                  "Here rhs shall not be a pointer");
        auto* lhs = getAddress(*expr.lhs());
        SC_ASSERT(isa<ir::PointerType>(lhs->type()),
                  "Need pointer to assign to variable");
        auto* structType =
            cast<sema::StructureType const*>(expr.lhs()->type()->base());
        auto* assignee = [&]() -> ir::Value* {
            if (expr.operation() == Assignment) {
                return rhs;
            }
            auto* lhsValue = loadAddress(lhs, mapType(structType), "lhs.value");
            auto* arithmeticResult =
                new ir::ArithmeticInst(lhsValue,
                                       rhs,
                                       mapArithmeticAssignOp(structType,
                                                             expr.operation()),
                                       "expr.result");
            currentBB()->pushBack(arithmeticResult);
            return arithmeticResult;
        }();
        auto* store = new ir::Store(irCtx, lhs, assignee);
        currentBB()->pushBack(store);
        return store;
    }
    case BinaryOperator::_count:
        SC_UNREACHABLE();
    }
}

ir::Value* CodeGenContext::getValueImpl(MemberAccess const& expr) {
    return loadAddress(getAddressImpl(expr),
                       mapType(expr.type()),
                       "member.access");
}

ir::Value* CodeGenContext::getValueImpl(ReferenceExpression const& expr) {
    SC_ASSERT(expr.referred()->type()->isReference() ||
                  expr.referred()->isLValue(),
              "");
    return getAddress(*expr.referred());
}

ir::Value* CodeGenContext::getValueImpl(UniqueExpression const& expr) {
    SC_DEBUGFAIL();
}

ir::Value* CodeGenContext::getValueImpl(Conditional const& condExpr) {
    auto* cond      = getValue(*condExpr.condition());
    auto* thenBlock = new ir::BasicBlock(irCtx, "conditional.then");
    auto* elseBlock = new ir::BasicBlock(irCtx, "conditional.else");
    auto* endBlock  = new ir::BasicBlock(irCtx, "conditional.end");
    currentBB()->pushBack(new ir::Branch(irCtx, cond, thenBlock, elseBlock));
    currentFunction->pushBack(thenBlock);
    /// Generate then block.
    setCurrentBB(thenBlock);
    auto* thenVal = getValue(*condExpr.thenExpr());
    thenBlock     = currentBB();
    thenBlock->pushBack(new ir::Goto(irCtx, endBlock));
    currentFunction->pushBack(elseBlock);
    /// Generate else block.
    setCurrentBB(elseBlock);
    auto* elseVal = getValue(*condExpr.elseExpr());
    elseBlock     = currentBB();
    elseBlock->pushBack(new ir::Goto(irCtx, endBlock));
    currentFunction->pushBack(endBlock);
    /// Generate end block.
    setCurrentBB(endBlock);
    auto* result =
        new ir::Phi({ { thenBlock, thenVal }, { elseBlock, elseVal } },
                    "conditional.result");
    currentBB()->pushBack(result);
    return result;
}

ir::Value* CodeGenContext::getValueImpl(FunctionCall const& expr) {
    auto* call = genCallImpl(expr);
    if (expr.type()->isReference()) {
        return loadAddress(call, mapType(expr.type()->base()), "call.value");
    }
    return call;
}

ir::Value* CodeGenContext::genCallImpl(FunctionCall const& expr) {
    ir::Callable* function = functionMap.find(expr.function())->second;
    auto args              = mapArguments(expr.arguments());
    if (expr.isMemberCall) {
        auto* object = cast<MemberAccess const*>(expr.object())->object();
        args.insert(args.begin(), getAddress(*object));
    }
    auto* call =
        new ir::Call(function,
                     args,
                     expr.type()->base() != symTable.Void() ? "call.result" :
                                                              std::string{});
    currentBB()->pushBack(call);
    return call;
}

ir::Value* CodeGenContext::getValueImpl(Subscript const& expr) {
    auto* address = getAddress(expr);
    return loadAddress(address, mapType(expr.type()->base()), "[].value");
}

ir::Value* CodeGenContext::getValueImpl(ImplicitConversion const& conv) {
    return getValue(*conv.expression());
    auto* expr = conv.expression();
    if (conv.type()->isReference()) {
        return getAddress(*expr);
    }
    return getValue(*expr);
}

ir::Value* CodeGenContext::getValueImpl(ListExpression const& list) {
    SC_DEBUGFAIL();
}

ir::Value* CodeGenContext::storeTmpToMemory(ir::Value* value,
                                            ir::Type const* type,
                                            std::string name) {
    SC_ASSERT(value || type, "");
    if (name.empty() && value) {
        name = utl::strcat(value->name(), ".addr");
    }
    if (value && type) {
        SC_ASSERT(value->type() == type, "");
    }
    if (!type) {
        type = value->type();
    }
    auto* addr = new ir::Alloca(irCtx, type, name);
    addAlloca(addr);
    if (!value) {
        return addr;
    }
    auto* store = new ir::Store(irCtx, addr, value);
    currentBB()->pushBack(store);
    return addr;
}

ir::Value* CodeGenContext::getAddressImpl(Expression const& expr) {
    SC_DEBUGFAIL();
}

ir::Value* CodeGenContext::getAddressImpl(Literal const& lit) {
    SC_ASSERT(lit.kind() == LiteralKind::This, "");
    auto* result = variableAddressMap[lit.entity()];
    SC_ASSERT(result, "");
    return result;
}

ir::Value* CodeGenContext::getAddressImpl(Identifier const& id) {
    auto* address = variableAddressMap[id.entity()];
    SC_ASSERT(address, "Undeclared identifier?");
    SC_ASSERT(id.type()->isImplicitReference() || id.isLValue(),
              "Just to be safe");
    return address;
}

ir::Value* CodeGenContext::getAddressImpl(MemberAccess const& expr) {
    /// Get the value or the address based on wether the base object is an
    /// l-value or r-value
    ir::Value* basePtr = [&]() -> ir::Value* {
        if (isArray(expr.object())) {
            return getReferenceAddress(*expr.object());
        }
        if (expr.object()->type()->isReference() || expr.object()->isLValue()) {
            return getAddress(*expr.object());
        }
        /// If we are an r-value we store the value to memory and return a
        /// pointer to it.
        auto* value = getValue(*expr.object());
        return storeTmpToMemory(value);
    }();
    auto* accessedElement = cast<Identifier const&>(*expr.member()).entity();
    auto* var             = cast<sema::Variable const*>(accessedElement);
    auto* type            = mapType(expr.object()->type()->base());
    if (isArray(expr.object())) {
        /// Hack for now to get arrays working
        type = arrayViewType;
    }
    auto* const gep = new ir::GetElementPointer(irCtx,
                                                type,
                                                basePtr,
                                                irCtx.integralConstant(0, 64),
                                                { var->index() },
                                                "member.ptr");
    currentBB()->pushBack(gep);
    return gep;
}

ir::Value* CodeGenContext::getAddressImpl(FunctionCall const& expr) {
    SC_ASSERT(expr.type()->isReference(),
              "Can't be l-value so we expect a reference");
    return genCallImpl(expr);
}

ir::Value* CodeGenContext::getAddressImpl(Subscript const& expr) {
    auto* arrayType =
        cast<sema::ArrayType const*>(expr.object()->type()->base());
    auto* elemType = mapType(arrayType->elementType());
    auto* dataPtr  = [&]() -> ir::Value* {
        auto* arrayAddress = getAddress(*expr.object());
        auto* dataPtr = new ir::ExtractValue(arrayAddress, { 0 }, "array.data");
        currentBB()->pushBack(dataPtr);
        return dataPtr;
    }();
    auto* index = getValue(*expr.arguments().front());
    auto* gep   = new ir::GetElementPointer(irCtx,
                                          elemType,
                                          dataPtr,
                                          index,
                                            {},
                                          "elem.ptr");
    currentBB()->pushBack(gep);
    return gep;
}

ir::Value* CodeGenContext::getAddressImpl(ReferenceExpression const& expr) {
    return getAddress(*expr.referred());
}

ir::Value* CodeGenContext::getAddressImpl(ImplicitConversion const& conv) {
    return visit(*conv.expression(),
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* CodeGenContext::getAddressImpl(ListExpression const& list) {
    auto* arrayType = cast<sema::ArrayType const*>(list.type()->base());
    memorizeVariableValue(arrayType->countVariable(),
                          irCtx.integralConstant(
                              APInt(arrayType->count(), 64)));
    auto* type  = mapType(arrayType->elementType());
    auto* array = new ir::Alloca(irCtx,
                                 irCtx.integralConstant(
                                     APInt(list.elements().size(), 32)),
                                 type,
                                 "array");
    addAlloca(array);
    for (auto [index, elem]: list.elements() | ranges::views::enumerate) {
        auto* elemValue = getValue(*elem);
        auto* gep =
            new ir::GetElementPointer(irCtx,
                                      type,
                                      array,
                                      irCtx.integralConstant(APInt(index, 32)),
                                      {},
                                      "elem.ptr");
        currentBB()->pushBack(gep);
        auto* store = new ir::Store(irCtx, gep, elemValue);
        currentBB()->pushBack(store);
    }
    return array;
}

ir::Value* CodeGenContext::loadAddress(ir::Value* address,
                                       ir::Type const* type,
                                       std::string_view name) {
    SC_ASSERT(isa<ir::PointerType>(address->type()),
              "We need a pointer to load from");
    auto* const load = new ir::Load(address, type, std::string(name));
    currentBB()->pushBack(load);
    return load;
}

ir::Value* CodeGenContext::makeArrayView(ir::Value* address, ir::Value* count) {
    auto* ins1 = new ir::InsertValue(irCtx.undef(arrayViewType),
                                     address,
                                     { 0 },
                                     "arrayview");
    auto* ins2 = new ir::InsertValue(ins1, count, { 1 }, "arrayview");
    currentBB()->pushBack(ins1);
    currentBB()->pushBack(ins2);
    return ins2;
}

void CodeGenContext::declareTypes() {
    for (sema::StructureType const* objType: symTable.sortedStructureTypes()) {
        auto structure = allocate<ir::StructureType>(objType->mangledName());
        for (sema::Variable const* member: objType->memberVariables()) {
            structure->addMember(mapType(member->type()));
        }
        typeMap[objType] = structure.get();
        mod.addStructure(std::move(structure));
    }
    /// Declare array view type
    auto structure = allocate<ir::StructureType>("[]");
    structure->addMember(irCtx.pointerType());
    structure->addMember(irCtx.integralType(64));
    arrayViewType = structure.get();
    mod.addStructure(std::move(structure));
}

static ir::FunctionAttribute translateAttrs(sema::FunctionAttribute attr) {
    switch (attr) {
        using enum ir::FunctionAttribute;
    case sema::FunctionAttribute::Pure:
        return Memory_WriteNone;
    case sema::FunctionAttribute::Const:
        return Memory_ReadNone | Memory_WriteNone;
    default:
        return None;
    }
}

static ir::Visibility accessSpecToVisibility(sema::AccessSpecifier spec) {
    switch (spec) {
    case sema::AccessSpecifier::Public:
        return ir::Visibility::Extern;
    case sema::AccessSpecifier::Private:
        return ir::Visibility::Static;
    }
}

void CodeGenContext::declareFunctions() {
    for (sema::Function const* function: symTable.functions()) {
        auto paramTypes = mapParameterTypes(function->argumentTypes());
        // TODO: Generate proper function type here
        ir::FunctionType const* const functionType = nullptr;
        if (function->isExtern()) {
            auto fn = allocate<ir::ExtFunction>(
                functionType,
                mapType(function->signature().returnType()),
                paramTypes,
                std::string(function->name()),
                utl::narrow_cast<uint32_t>(function->slot()),
                utl::narrow_cast<uint32_t>(function->index()),
                translateAttrs(function->attributes()));
            functionMap[function] = fn.get();
            mod.addGlobal(std::move(fn));
        }
        else {
            auto fn =
                allocate<ir::Function>(functionType,
                                       mapType(
                                           function->signature().returnType()),
                                       paramTypes,
                                       function->mangledName(),
                                       translateAttrs(function->attributes()),
                                       accessSpecToVisibility(
                                           function->accessSpecifier()));
            functionMap[function] = fn.get();
            mod.addFunction(std::move(fn));
        }
    }
}

void CodeGenContext::setCurrentBB(ir::BasicBlock* bb) { _currentBB = bb; }

void CodeGenContext::memorizeVariableAddress(sema::Entity const* entity,
                                             ir::Value* value) {
    SC_ASSERT(!variableAddressMap.contains(entity),
              "Variable id must not be declared multiple times. This error "
              "must be handled in sema.");
    variableAddressMap[entity] = value;
}

void CodeGenContext::memorizeVariableValue(sema::Entity const* entity,
                                           ir::Value* value) {
    SC_ASSERT(!variableValueMap.contains(entity),
              "Variable id must not be declared multiple times. This error "
              "must be handled in sema.");
    variableValueMap[entity] = value;
}

ir::Type const* CodeGenContext::mapType(sema::Type const* semaType) {
    // clang-format off
    return visit(*semaType, utl::overload{
        [&](sema::QualType const& qualType) -> ir::Type const* {
            if (qualType.isReference()) {
                if (isa<sema::ArrayType>(qualType.base())) {
                    return arrayViewType;
                }
                return irCtx.pointerType();
            }
            return mapType(qualType.base());
        },
        [&](sema::StructureType const& structType) -> ir::Type const* {
            if (&structType == symTable.Void()) {
                return irCtx.voidType();
            }
            if (&structType == symTable.Byte()) {
                return irCtx.integralType(8);
            }
            if (&structType == symTable.Bool()) {
                return irCtx.integralType(1);
            }
            if (&structType == symTable.Int()) {
                return irCtx.integralType(64);
            }
            if (&structType == symTable.Float()) {
                return irCtx.floatType(64);
            }
            if (auto itr = typeMap.find(&structType); itr != typeMap.end()) {
                return itr->second;
            }
            SC_DEBUGFAIL();
        },
        [&](sema::ArrayType const& arrayType) {
            return arrayViewType;
        },
    }); // clang-format on
}

utl::small_vector<ir::Type const*> CodeGenContext::mapParameterTypes(
    std::span<sema::QualType const* const> types) {
    utl::small_vector<ir::Type const*> result;
    for (auto* semaType: types) {
        result.push_back(mapType(semaType));
    }
    return result;
}

utl::small_vector<ir::Value*> CodeGenContext::mapArguments(
    ranges::range auto&& args) {
    utl::small_vector<ir::Value*> result;
    for (auto* expr: args) {
        auto* value = getValue(*expr);
        result.push_back(value);
    }
    return result;
}

void CodeGenContext::generateParameter(ir::BasicBlock* entry,
                                       List<ir::Parameter>::iterator& paramItr,
                                       ParameterDeclaration const* paramDecl) {
    auto const name = paramDecl->name();
    auto* semaType  = paramDecl->type();
    auto* address = new ir::Alloca(irCtx, mapType(semaType), std::string(name));
    addAlloca(address);
    memorizeVariableAddress(paramDecl->entity(), address);
    auto* store = new ir::Store(irCtx, address, paramItr++.to_address());
    entry->pushBack(store);
}

ir::UnaryArithmeticOperation CodeGenContext::mapUnaryArithmeticOp(
    ast::UnaryPrefixOperator op) const {
    switch (op) {
    case UnaryPrefixOperator::BitwiseNot:
        return ir::UnaryArithmeticOperation::BitwiseNot;
    case UnaryPrefixOperator::LogicalNot:
        return ir::UnaryArithmeticOperation::LogicalNot;
    default:
        SC_UNREACHABLE();
    }
}

ir::CompareMode CodeGenContext::mapCompareMode(
    sema::StructureType const* type) const {
    if (type == symTable.Bool()) {
        return ir::CompareMode::Unsigned;
    }
    if (type == symTable.Int()) {
        return ir::CompareMode::Signed;
    }
    if (type == symTable.Float()) {
        return ir::CompareMode::Float;
    }
    SC_DEBUGFAIL();
}

ir::CompareOperation CodeGenContext::mapCompareOp(
    ast::BinaryOperator op) const {
    switch (op) {
    case BinaryOperator::Less:
        return ir::CompareOperation::Less;
    case BinaryOperator::LessEq:
        return ir::CompareOperation::LessEq;
    case BinaryOperator::Greater:
        return ir::CompareOperation::Greater;
    case BinaryOperator::GreaterEq:
        return ir::CompareOperation::GreaterEq;
    case BinaryOperator::Equals:
        return ir::CompareOperation::Equal;
    case BinaryOperator::NotEquals:
        return ir::CompareOperation::NotEqual;
    default:
        SC_UNREACHABLE("Only handle compare operations here.");
    }
}

ir::ArithmeticOperation CodeGenContext::mapArithmeticOp(
    sema::StructureType const* type, ast::BinaryOperator op) const {
    switch (op) {
    case BinaryOperator::Multiplication:
        if (type == symTable.Int()) {
            return ir::ArithmeticOperation::Mul;
        }
        if (type == symTable.Float()) {
            return ir::ArithmeticOperation::FMul;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Division:
        if (type == symTable.Int()) {
            return ir::ArithmeticOperation::SDiv;
        }
        if (type == symTable.Float()) {
            return ir::ArithmeticOperation::FDiv;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Remainder:
        if (type == symTable.Int()) {
            return ir::ArithmeticOperation::SRem;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Addition:
        if (type == symTable.Int()) {
            return ir::ArithmeticOperation::Add;
        }
        if (type == symTable.Float()) {
            return ir::ArithmeticOperation::FAdd;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Subtraction:
        if (type == symTable.Int()) {
            return ir::ArithmeticOperation::Sub;
        }
        if (type == symTable.Float()) {
            return ir::ArithmeticOperation::FSub;
        }
        SC_UNREACHABLE();
    case BinaryOperator::LeftShift:
        return ir::ArithmeticOperation::LShL;
    case BinaryOperator::RightShift:
        return ir::ArithmeticOperation::LShR;
    case BinaryOperator::BitwiseAnd:
        return ir::ArithmeticOperation::And;
    case BinaryOperator::BitwiseXOr:
        return ir::ArithmeticOperation::XOr;
    case BinaryOperator::BitwiseOr:
        return ir::ArithmeticOperation::Or;
    default:
        SC_UNREACHABLE("Only handle arithmetic operations here.");
    }
}

ir::ArithmeticOperation CodeGenContext::mapArithmeticAssignOp(
    sema::StructureType const* type, ast::BinaryOperator op) const {
    auto nonAssign = [&] {
        switch (op) {
        case BinaryOperator::AddAssignment:
            return BinaryOperator::Addition;
        case BinaryOperator::SubAssignment:
            return BinaryOperator::Subtraction;
        case BinaryOperator::MulAssignment:
            return BinaryOperator::Multiplication;
        case BinaryOperator::DivAssignment:
            return BinaryOperator::Division;
        case BinaryOperator::RemAssignment:
            return BinaryOperator::Remainder;
        case BinaryOperator::LSAssignment:
            return BinaryOperator::LeftShift;
        case BinaryOperator::RSAssignment:
            return BinaryOperator::RightShift;
        case BinaryOperator::AndAssignment:
            return BinaryOperator::BitwiseAnd;
        case BinaryOperator::OrAssignment:
            return BinaryOperator::BitwiseOr;
        case BinaryOperator::XOrAssignment:
            return BinaryOperator::BitwiseXOr;
        default:
            SC_UNREACHABLE("Only handle arithmetic assign operations here.");
        }
    }();
    return mapArithmeticOp(type, nonAssign);
}
