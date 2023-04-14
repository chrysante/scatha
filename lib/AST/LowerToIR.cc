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
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

namespace {

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
    void generateImpl(WhileStatement const&);
    void generateImpl(DoWhileStatement const&);
    void generateImpl(ForStatement const&);

    ir::Value* getValue(Expression const& expr);

    ir::Value* getValueImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getValueImpl(Expression const& expr) { SC_UNREACHABLE(); }
    ir::Value* getValueImpl(Identifier const&);
    ir::Value* getValueImpl(IntegerLiteral const&);
    ir::Value* getValueImpl(BooleanLiteral const&);
    ir::Value* getValueImpl(FloatingPointLiteral const&);
    ir::Value* getValueImpl(StringLiteral const&);
    ir::Value* getValueImpl(UnaryPrefixExpression const&);
    ir::Value* getValueImpl(BinaryExpression const&);
    ir::Value* getValueImpl(MemberAccess const&);
    ir::Value* getValueImpl(Conditional const&);
    ir::Value* getValueImpl(FunctionCall const&);
    ir::Value* getValueImpl(Subscript const&);

    ir::Value* getAddress(Expression const& node);

    ir::Value* getAddressImpl(AbstractSyntaxTree const& expr) {
        SC_UNREACHABLE();
    } // Delete this later
    ir::Value* getAddressImpl(Expression const& expr) { SC_UNREACHABLE(); }
    ir::Value* getAddressImpl(Identifier const&);
    ir::Value* getAddressImpl(MemberAccess const&);

    ir::Value* loadAddress(ir::Value* address,
                           ir::Type const* type,
                           std::string_view name);

    void declareTypes();
    void declareFunctions();

    void addAlloca(ir::Alloca* allc) {
        allocaInsertItr =
            currentFunction->entry().insert(allocaInsertItr, allc)->next();
    }

    ir::BasicBlock* currentBB() { return _currentBB; }
    void setCurrentBB(ir::BasicBlock*);

    void memorizeVariableAddress(sema::SymbolID, ir::Value*);

    std::string mangledName(sema::SymbolID) const;
    std::string mangledName(sema::SymbolID, std::string_view name) const;
    ir::Type const* mapType(sema::TypeID semaTypeID);
    ir::UnaryArithmeticOperation mapUnaryArithmeticOp(
        ast::UnaryPrefixOperator) const;
    ir::CompareMode mapCompareMode(sema::TypeID) const;
    ir::CompareOperation mapCompareOp(ast::BinaryOperator) const;
    ir::ArithmeticOperation mapArithmeticOp(sema::TypeID typeID,
                                            ast::BinaryOperator) const;
    ir::ArithmeticOperation mapArithmeticAssignOp(sema::TypeID typeID,
                                                  ast::BinaryOperator) const;

    ir::Context& irCtx;
    ir::Module& mod;
    sema::SymbolTable const& symTable;
    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* _currentBB    = nullptr;
    utl::hashmap<sema::SymbolID, ir::Value*> variableAddressMap;
    utl::hashmap<sema::SymbolID, ir::Callable*> functionMap;
    utl::hashmap<sema::TypeID, ir::Type const*> typeMap;
    ir::Instruction* allocaInsertItr;
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
    for (auto& decl: tu.declarations) {
        generate(*decl);
    }
}

void CodeGenContext::generateImpl(CompoundStatement const& cmpStmt) {
    for (auto& statement: cmpStmt.statements) {
        generate(*statement);
    }
}

void CodeGenContext::generateImpl(FunctionDefinition const& def) {
    auto paramTypes = def.parameters |
                      ranges::views::transform([&](auto& param) {
                          return mapType(param->typeID());
                      }) |
                      ranges::to<utl::small_vector<ir::Type const*>>;
    auto* fn = cast<ir::Function*>(functionMap.find(def.symbolID())->second);
    currentFunction = fn;
    auto* entry     = new ir::BasicBlock(irCtx, "entry");
    fn->pushBack(entry);
    setCurrentBB(entry);
    allocaInsertItr = entry->begin().to_address();
    for (auto paramItr = fn->parameters().begin();
         auto& paramDecl: def.parameters)
    {
        auto* paramMemPtr = new ir::Alloca(irCtx,
                                           mapType(paramDecl->typeID()),
                                           std::string(paramDecl->name()));
        addAlloca(paramMemPtr);
        memorizeVariableAddress(paramDecl->symbolID(), paramMemPtr);
        auto* store =
            new ir::Store(irCtx, paramMemPtr, std::to_address(paramItr++));
        entry->pushBack(store);
    }
    generate(*def.body);
    setCurrentBB(nullptr);
    currentFunction = nullptr;
}

void CodeGenContext::generateImpl(StructDefinition const& def) {
    /// Nothing to do here, structs are handled separately.
}

void CodeGenContext::generateImpl(VariableDeclaration const& varDecl) {
    auto* varMemPtr = new ir::Alloca(irCtx,
                                     mapType(varDecl.typeID()),
                                     std::string(varDecl.name()));
    addAlloca(varMemPtr);
    memorizeVariableAddress(varDecl.symbolID(), varMemPtr);
    if (varDecl.initExpression == nullptr) {
        return;
    }
    ir::Value* initValue = getValue(*varDecl.initExpression);
    auto* store          = new ir::Store(irCtx, varMemPtr, initValue);
    currentBB()->pushBack(store);
}

void CodeGenContext::generateImpl(ParameterDeclaration const&) {
    SC_UNREACHABLE("Handled by generate(FunctionDefinition)");
}

void CodeGenContext::generateImpl(ExpressionStatement const& exprStatement) {
    (void)getValue(*exprStatement.expression);
}

void CodeGenContext::generateImpl(EmptyStatement const& empty) {
    /// Nothing to do here.
}

void CodeGenContext::generateImpl(ReturnStatement const& retDecl) {
    auto* returnValue =
        retDecl.expression ? getValue(*retDecl.expression) : irCtx.voidValue();
    auto* ret = new ir::Return(irCtx, returnValue);
    currentBB()->pushBack(ret);
}

void CodeGenContext::generateImpl(IfStatement const& ifStatement) {
    auto* condition = getValue(*ifStatement.condition);
    auto* thenBlock = new ir::BasicBlock(irCtx, "if.then");
    auto* elseBlock =
        ifStatement.elseBlock ? new ir::BasicBlock(irCtx, "if.else") : nullptr;
    auto* endBlock = new ir::BasicBlock(irCtx, "if.end");
    auto* branch   = new ir::Branch(irCtx,
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
    addBlock(thenBlock, *ifStatement.thenBlock);
    if (ifStatement.elseBlock) {
        addBlock(elseBlock, *ifStatement.elseBlock);
    }
    currentFunction->pushBack(endBlock);
    setCurrentBB(endBlock);
}

void CodeGenContext::generateImpl(WhileStatement const& loopDecl) {
    auto* loopHeader = new ir::BasicBlock(irCtx, "loop.header");
    currentFunction->pushBack(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, "loop.body");
    currentFunction->pushBack(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, "loop.end");
    currentFunction->pushBack(loopEnd);
    auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
    currentBB()->pushBack(gotoLoopHeader);
    setCurrentBB(loopHeader);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->pushBack(branch);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
    currentBB()->pushBack(gotoLoopHeader2);
    setCurrentBB(loopEnd);
}

void CodeGenContext::generateImpl(DoWhileStatement const& loopDecl) {
    auto* loopBody = new ir::BasicBlock(irCtx, "loop.body");
    currentFunction->pushBack(loopBody);
    auto* loopFooter = new ir::BasicBlock(irCtx, "loop.footer");
    currentFunction->pushBack(loopFooter);
    auto* loopEnd = new ir::BasicBlock(irCtx, "loop.end");
    currentFunction->pushBack(loopEnd);
    auto* gotoLoopBody = new ir::Goto(irCtx, loopBody);
    currentBB()->pushBack(gotoLoopBody);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    auto* gotoLoopFooter = new ir::Goto(irCtx, loopFooter);
    currentBB()->pushBack(gotoLoopFooter);
    setCurrentBB(loopFooter);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->pushBack(branch);
    setCurrentBB(loopEnd);
}

void CodeGenContext::generateImpl(ForStatement const& loopDecl) {
    auto* loopHeader = new ir::BasicBlock(irCtx, "loop.header");
    currentFunction->pushBack(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, "loop.body");
    currentFunction->pushBack(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, "loop.end");
    currentFunction->pushBack(loopEnd);
    generate(*loopDecl.varDecl);
    auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
    currentBB()->pushBack(gotoLoopHeader);
    setCurrentBB(loopHeader);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->pushBack(branch);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    getValue(*loopDecl.increment);
    auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
    currentBB()->pushBack(gotoLoopHeader2);
    setCurrentBB(loopEnd);
}

ir::Value* CodeGenContext::getValue(Expression const& expr) {
    return visit(expr, [this](auto const& expr) { return getValueImpl(expr); });
}

ir::Value* CodeGenContext::getValueImpl(Identifier const& id) {
    return loadAddress(getAddressImpl(id), mapType(id.typeID()), id.value());
}

ir::Value* CodeGenContext::getValueImpl(IntegerLiteral const& intLit) {
    return irCtx.integralConstant(intLit.value());
}

ir::Value* CodeGenContext::getValueImpl(BooleanLiteral const& boolLit) {
    return irCtx.integralConstant(boolLit.value());
}

ir::Value* CodeGenContext::getValueImpl(FloatingPointLiteral const& floatLit) {
    return irCtx.floatConstant(floatLit.value(), 64);
}

ir::Value* CodeGenContext::getValueImpl(StringLiteral const&) {
    SC_DEBUGFAIL();
}

ir::Value* CodeGenContext::getValueImpl(UnaryPrefixExpression const& expr) {
    if (expr.operation() == ast::UnaryPrefixOperator::Increment ||
        expr.operation() == ast::UnaryPrefixOperator::Decrement)
    {
        ir::Value* addr      = getAddress(*expr.operand);
        ir::Type const* type = mapType(expr.operand->typeID());
        ir::Value* value =
            loadAddress(addr, type, utl::strcat(expr.operation(), ".value"));
        auto const operation =
            expr.operation() == ast::UnaryPrefixOperator::Increment ?
                ir::ArithmeticOperation::Add :
                ir::ArithmeticOperation::Sub;
        auto* arithmetic =
            new ir::ArithmeticInst(value,
                                   irCtx.integralConstant(APInt(1, 64)),
                                   operation,
                                   utl::strcat(expr.operation(), ".result"));
        currentBB()->pushBack(arithmetic);
        auto* store = new ir::Store(irCtx, addr, arithmetic);
        currentBB()->pushBack(store);
        return arithmetic;
    }
    ir::Value* const operand = getValue(*expr.operand);
    if (expr.operation() == ast::UnaryPrefixOperator::Promotion) {
        return operand;
    }
    else if (expr.operation() == ast::UnaryPrefixOperator::Negation) {
        auto* type = operand->type();
        auto* inst = new ir::ArithmeticInst(irCtx.arithmeticConstant(0, type),
                                            operand,
                                            isa<ir::IntegralType>(type) ?
                                                ir::ArithmeticOperation::Sub :
                                                ir::ArithmeticOperation::FSub,
                                            std::string("expr.result"));
        currentBB()->pushBack(inst);
        return inst;
    }
    else {
        auto* inst =
            new ir::UnaryArithmeticInst(irCtx,
                                        operand,
                                        mapUnaryArithmeticOp(expr.operation()),
                                        std::string("expr.result"));
        currentBB()->pushBack(inst);
        return inst;
    }
}

ir::Value* CodeGenContext::getValueImpl(BinaryExpression const& exprDecl) {
    switch (exprDecl.operation()) {
    case BinaryOperator::Multiplication:
        [[fallthrough]];
    case BinaryOperator::Division:
        [[fallthrough]];
    case BinaryOperator::Remainder:
        [[fallthrough]];
    case BinaryOperator::Addition:
        [[fallthrough]];
    case BinaryOperator::Subtraction:
        [[fallthrough]];
    case BinaryOperator::LeftShift:
        [[fallthrough]];
    case BinaryOperator::RightShift:
        [[fallthrough]];
    case BinaryOperator::BitwiseAnd:
        [[fallthrough]];
    case BinaryOperator::BitwiseXOr:
        [[fallthrough]];
    case BinaryOperator::BitwiseOr: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        ir::Value* const rhs = getValue(*exprDecl.rhs);
        auto* arithInst =
            new ir::ArithmeticInst(lhs,
                                   rhs,
                                   mapArithmeticOp(exprDecl.typeID(),
                                                   exprDecl.operation()),
                                   "expr.result");
        currentBB()->pushBack(arithInst);
        return arithInst;
    }
    case BinaryOperator::LogicalAnd:
        [[fallthrough]];
    case BinaryOperator::LogicalOr: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        auto* startBlock     = currentBB();
        auto* rhsBlock       = new ir::BasicBlock(irCtx, "logical.rhs");
        auto* endBlock       = new ir::BasicBlock(irCtx, "logical.end");
        currentBB()->pushBack(
            exprDecl.operation() == BinaryOperator::LogicalAnd ?
                new ir::Branch(irCtx, lhs, rhsBlock, endBlock) :
                new ir::Branch(irCtx, lhs, endBlock, rhsBlock));
        currentFunction->pushBack(rhsBlock);
        setCurrentBB(rhsBlock);
        auto* rhs = getValue(*exprDecl.rhs);
        currentBB()->pushBack(new ir::Goto(irCtx, endBlock));
        currentFunction->pushBack(endBlock);
        setCurrentBB(endBlock);
        auto* result =
            exprDecl.operation() == BinaryOperator::LogicalAnd ?
                new ir::Phi({ { startBlock, irCtx.integralConstant(0, 1) },
                              { rhsBlock, rhs } },
                            "logical.and.value") :
                new ir::Phi({ { startBlock, irCtx.integralConstant(1, 1) },
                              { rhsBlock, rhs } },
                            "logical.or.value");
        currentBB()->pushBack(result);
        return result;
    }
    case BinaryOperator::Less:
        [[fallthrough]];
    case BinaryOperator::LessEq:
        [[fallthrough]];
    case BinaryOperator::Greater:
        [[fallthrough]];
    case BinaryOperator::GreaterEq:
        [[fallthrough]];
    case BinaryOperator::Equals:
        [[fallthrough]];
    case BinaryOperator::NotEquals: {
        auto* cmpInst =
            new ir::CompareInst(irCtx,
                                getValue(*exprDecl.lhs),
                                getValue(*exprDecl.rhs),
                                mapCompareMode(exprDecl.lhs->typeID()),
                                mapCompareOp(exprDecl.operation()),
                                "cmp.result");
        currentBB()->pushBack(cmpInst);
        return cmpInst;
    }
    case BinaryOperator::Comma: {
        getValue(*exprDecl.lhs);
        return getValue(*exprDecl.rhs);
    }
    case BinaryOperator::Assignment: {
        ir::Value* lhsAddr      = getAddress(*exprDecl.lhs);
        ir::Type const* lhsType = mapType(exprDecl.lhs->typeID());
        ir::Value* rhs          = getValue(*exprDecl.rhs);
        auto* store             = new ir::Store(irCtx, lhsAddr, rhs);
        currentBB()->pushBack(store);
        return loadAddress(lhsAddr, lhsType, "tmp");
    }
    case BinaryOperator::AddAssignment:
        [[fallthrough]];
    case BinaryOperator::SubAssignment:
        [[fallthrough]];
    case BinaryOperator::MulAssignment:
        [[fallthrough]];
    case BinaryOperator::DivAssignment:
        [[fallthrough]];
    case BinaryOperator::RemAssignment:
        [[fallthrough]];
    case BinaryOperator::LSAssignment:
        [[fallthrough]];
    case BinaryOperator::RSAssignment:
        [[fallthrough]];
    case BinaryOperator::AndAssignment:
        [[fallthrough]];
    case BinaryOperator::OrAssignment:
        [[fallthrough]];
    case BinaryOperator::XOrAssignment: {
        ir::Value* lhsAddr      = getAddress(*exprDecl.lhs);
        ir::Type const* lhsType = mapType(exprDecl.lhs->typeID());
        ir::Value* lhs          = loadAddress(lhsAddr, lhsType, "lhs");
        ir::Value* rhs          = getValue(*exprDecl.rhs);
        auto* result =
            new ir::ArithmeticInst(lhs,
                                   rhs,
                                   mapArithmeticAssignOp(exprDecl.lhs->typeID(),
                                                         exprDecl.operation()),
                                   "expr.result");
        currentBB()->pushBack(result);
        auto* store = new ir::Store(irCtx, lhsAddr, result);
        currentBB()->pushBack(store);
        return loadAddress(lhsAddr, lhsType, "tmp");
    }
    case BinaryOperator::_count:
        SC_DEBUGFAIL();
    }
}

ir::Value* CodeGenContext::getValueImpl(MemberAccess const& expr) {
    return loadAddress(getAddressImpl(expr),
                       mapType(expr.typeID()),
                       "member.access");
}

ir::Value* CodeGenContext::getValueImpl(Conditional const& condExpr) {
    auto* cond      = getValue(*condExpr.condition);
    auto* thenBlock = new ir::BasicBlock(irCtx, "conditional.then");
    auto* elseBlock = new ir::BasicBlock(irCtx, "conditional.else");
    auto* endBlock  = new ir::BasicBlock(irCtx, "conditional.end");
    currentBB()->pushBack(new ir::Branch(irCtx, cond, thenBlock, elseBlock));
    currentFunction->pushBack(thenBlock);
    /// Generate then block.
    setCurrentBB(thenBlock);
    auto* thenVal = getValue(*condExpr.ifExpr);
    thenBlock     = currentBB();
    thenBlock->pushBack(new ir::Goto(irCtx, endBlock));
    currentFunction->pushBack(elseBlock);
    /// Generate else block.
    setCurrentBB(elseBlock);
    auto* elseVal = getValue(*condExpr.elseExpr);
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

ir::Value* CodeGenContext::getValueImpl(FunctionCall const& functionCall) {
    ir::Callable* function =
        functionMap.find(functionCall.functionID())->second;
    auto const args =
        functionCall.arguments |
        ranges::views::transform(
            [this](auto& expr) -> ir::Value* { return getValue(*expr); }) |
        ranges::to<utl::small_vector<ir::Value*>>;
    auto* call =
        new ir::Call(function,
                     args,
                     functionCall.typeID() != symTable.Void() ? "call.result" :
                                                                std::string{});
    currentBB()->pushBack(call);
    return call;
}

ir::Value* CodeGenContext::getValueImpl(Subscript const&) { SC_DEBUGFAIL(); }

ir::Value* CodeGenContext::getAddress(Expression const& expr) {
    return visit(expr,
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* CodeGenContext::getAddressImpl(Identifier const& id) {
    auto itr = variableAddressMap.find(id.symbolID());
    SC_ASSERT(itr != variableAddressMap.end(), "Undeclared symbol");
    return itr->second;
}

ir::Value* CodeGenContext::getAddressImpl(MemberAccess const& expr) {
    /// Get the value or the address based on wether the base object is an
    /// l-value or r-value
    ir::Value* basePtr = [&]() -> ir::Value* {
        if (expr.object->valueCategory() == ast::ValueCategory::LValue) {
            return getAddress(*expr.object);
        }
        /// If we are an r-value we store the value to memory and return a
        /// pointer to it.
        auto* value = getValue(*expr.object);
        auto* addr  = new ir::Alloca(irCtx, value->type(), "tmp");
        addAlloca(addr);
        auto* store = new ir::Store(irCtx, addr, value);
        currentBB()->pushBack(store);
        return addr;
    }();
    sema::SymbolID const accessedElementID =
        cast<Identifier const&>(*expr.member).symbolID();
    auto& var       = symTable.getVariable(accessedElementID);
    auto* const gep = new ir::GetElementPointer(irCtx,
                                                mapType(expr.object->typeID()),
                                                basePtr,
                                                irCtx.integralConstant(0, 64),
                                                { var.index() },
                                                "member.ptr");
    currentBB()->pushBack(gep);
    return gep;
}

ir::Value* CodeGenContext::loadAddress(ir::Value* address,
                                       ir::Type const* type,
                                       std::string_view name) {
    auto* const load = new ir::Load(address, type, std::string(name));
    currentBB()->pushBack(load);
    return load;
}

void CodeGenContext::declareTypes() {
    for (sema::TypeID const& typeID: symTable.sortedObjectTypes()) {
        auto const& objType = symTable.getObjectType(typeID);
        auto structure      = allocate<ir::StructureType>(
            mangledName(objType.symbolID(), objType.name()));
        for (sema::SymbolID const memberVarID: objType.memberVariables()) {
            auto& varDecl = symTable.getVariable(memberVarID);
            structure->addMember(mapType(varDecl.typeID()));
        }
        typeMap[typeID] = structure.get();
        mod.addStructure(std::move(structure));
    }
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
    for (sema::Function const& function: symTable.functions()) {
        auto paramTypes =
            function.signature().argumentTypeIDs() |
            ranges::views::transform([&](sema::TypeID paramTypeID) {
                return mapType(paramTypeID);
            }) |
            ranges::to<utl::small_vector<ir::Type const*>>;
        // TODO: Generate proper function type here
        ir::FunctionType const* const functionType = nullptr;
        if (function.isExtern()) {
            auto fn = allocate<ir::ExtFunction>(
                functionType,
                mapType(function.signature().returnTypeID()),
                paramTypes,
                std::string(function.name()),
                utl::narrow_cast<uint32_t>(function.slot()),
                utl::narrow_cast<uint32_t>(function.index()),
                translateAttrs(function.attributes()));
            functionMap[function.symbolID()] = fn.get();
            mod.addGlobal(std::move(fn));
        }
        else {
            auto fn =
                allocate<ir::Function>(functionType,
                                       mapType(
                                           function.signature().returnTypeID()),
                                       paramTypes,
                                       mangledName(function.symbolID(),
                                                   function.name()),
                                       translateAttrs(function.attributes()),
                                       accessSpecToVisibility(
                                           function.accessSpecifier()));
            functionMap[function.symbolID()] = fn.get();
            mod.addFunction(std::move(fn));
        }
    }
}

void CodeGenContext::setCurrentBB(ir::BasicBlock* bb) { _currentBB = bb; }

void CodeGenContext::memorizeVariableAddress(sema::SymbolID symbolID,
                                             ir::Value* value) {
    [[maybe_unused]] auto const [_, insertSuccess] =
        variableAddressMap.insert({ symbolID, value });
    SC_ASSERT(insertSuccess,
              "Variable id must not be declared multiple times. This error "
              "must be handled in sema.");
}

std::string CodeGenContext::mangledName(sema::SymbolID id) const {
    return mangledName(id, symTable.getObjectType(id).name());
}

std::string CodeGenContext::mangledName(sema::SymbolID id,
                                        std::string_view name) const {
    std::stringstream sstr;
    sstr << name << std::hex << id.rawValue();
    return std::move(sstr).str();
}

ir::Type const* CodeGenContext::mapType(sema::TypeID semaTypeID) {
    if (semaTypeID == symTable.Void()) {
        return irCtx.voidType();
    }
    if (semaTypeID == symTable.Int()) {
        return irCtx.integralType(64);
    }
    if (semaTypeID == symTable.Bool()) {
        return irCtx.integralType(1);
    }
    if (semaTypeID == symTable.Float()) {
        return irCtx.floatType(64);
    }
    if (auto itr = typeMap.find(semaTypeID); itr != typeMap.end()) {
        return itr->second;
    }
    SC_DEBUGFAIL();
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

ir::CompareMode CodeGenContext::mapCompareMode(sema::TypeID typeID) const {
    if (typeID == symTable.Bool()) {
        return ir::CompareMode::Unsigned;
    }
    if (typeID == symTable.Int()) {
        return ir::CompareMode::Signed;
    }
    if (typeID == symTable.Float()) {
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
    sema::TypeID typeID, ast::BinaryOperator op) const {
    switch (op) {
    case BinaryOperator::Multiplication:
        if (typeID == symTable.Int()) {
            return ir::ArithmeticOperation::Mul;
        }
        if (typeID == symTable.Float()) {
            return ir::ArithmeticOperation::FMul;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Division:
        if (typeID == symTable.Int()) {
            return ir::ArithmeticOperation::SDiv;
        }
        if (typeID == symTable.Float()) {
            return ir::ArithmeticOperation::FDiv;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Remainder:
        if (typeID == symTable.Int()) {
            return ir::ArithmeticOperation::SRem;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Addition:
        if (typeID == symTable.Int()) {
            return ir::ArithmeticOperation::Add;
        }
        if (typeID == symTable.Float()) {
            return ir::ArithmeticOperation::FAdd;
        }
        SC_UNREACHABLE();
    case BinaryOperator::Subtraction:
        if (typeID == symTable.Int()) {
            return ir::ArithmeticOperation::Sub;
        }
        if (typeID == symTable.Float()) {
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
    sema::TypeID typeID, ast::BinaryOperator op) const {
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
    return mapArithmeticOp(typeID, nonAssign);
}
