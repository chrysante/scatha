#include "CodeGen/AST2IR/CodeGenerator.h"

#include <range/v3/view.hpp>
#include <utl/format.hpp>
#include <utl/stack.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
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

    std::string localUniqueName(auto const&... args);

    std::string mangledName(sema::SymbolID) const;
    std::string mangledName(sema::SymbolID, std::string_view name) const;
    ir::Type const* mapType(sema::TypeID semaTypeID);
    static ir::UnaryArithmeticOperation mapUnaryArithmeticOp(
        ast::UnaryPrefixOperator);
    static ir::CompareOperation mapCompareOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticAssignOp(ast::BinaryOperator);

    ir::Context& irCtx;
    ir::Module& mod;
    sema::SymbolTable const& symTable;
    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* _currentBB    = nullptr;
    utl::hashmap<sema::SymbolID, ir::Value*> valueMap;
    utl::hashmap<sema::TypeID, ir::Type const*> typeMap;
    ir::Instruction* allocaInsertItr;
};

} // namespace

ir::Module ast::codegen(AbstractSyntaxTree const& ast,
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
    // TODO: Also here worry about name mangling
    auto* fn = cast<ir::Function*>(
        irCtx.getGlobal(utl::strcat(def.name(), def.symbolID())));
    currentFunction = fn;
    auto* entry     = new ir::BasicBlock(irCtx, localUniqueName("entry"));
    fn->pushBack(entry);
    setCurrentBB(entry);
    allocaInsertItr = entry->begin().to_address();
    for (auto paramItr = fn->parameters().begin();
         auto& paramDecl: def.parameters)
    {
        auto* paramMemPtr = new ir::Alloca(irCtx,
                                           mapType(paramDecl->typeID()),
                                           localUniqueName(paramDecl->name()));
        addAlloca(paramMemPtr);
        memorizeVariableAddress(paramDecl->symbolID(), paramMemPtr);
        auto* store =
            new ir::Store(irCtx, paramMemPtr, std::to_address(paramItr++));
        entry->pushBack(store);
    }
    generate(*def.body);
    setCurrentBB(nullptr);
    currentFunction = nullptr;
    mod.addFunction(fn);
}

void CodeGenContext::generateImpl(StructDefinition const& def) {
    /// Nothing to do here, structs are handled separately.
}

void CodeGenContext::generateImpl(VariableDeclaration const& varDecl) {
    auto* varMemPtr = new ir::Alloca(irCtx,
                                     mapType(varDecl.typeID()),
                                     localUniqueName(varDecl.name()));
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
        retDecl.expression ? getValue(*retDecl.expression) : nullptr;
    auto* ret = new ir::Return(irCtx, returnValue);
    currentBB()->pushBack(ret);
}

void CodeGenContext::generateImpl(IfStatement const& ifStatement) {
    auto* condition = getValue(*ifStatement.condition);
    auto* thenBlock = new ir::BasicBlock(irCtx, localUniqueName("then"));
    auto* elseBlock = ifStatement.elseBlock ?
                          new ir::BasicBlock(irCtx, localUniqueName("else")) :
                          nullptr;
    auto* endBlock  = new ir::BasicBlock(irCtx, localUniqueName("if.end"));
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
    addBlock(thenBlock, *ifStatement.ifBlock);
    if (ifStatement.elseBlock) {
        addBlock(elseBlock, *ifStatement.elseBlock);
    }
    currentFunction->pushBack(endBlock);
    setCurrentBB(endBlock);
}

void CodeGenContext::generateImpl(WhileStatement const& loopDecl) {
    auto* loopHeader =
        new ir::BasicBlock(irCtx, localUniqueName("loop.header"));
    currentFunction->pushBack(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop.body"));
    currentFunction->pushBack(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop.end"));
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
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop.body"));
    currentFunction->pushBack(loopBody);
    auto* loopFooter =
        new ir::BasicBlock(irCtx, localUniqueName("loop.footer"));
    currentFunction->pushBack(loopFooter);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop.end"));
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
    auto* loopHeader =
        new ir::BasicBlock(irCtx, localUniqueName("loop.header"));
    currentFunction->pushBack(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop.body"));
    currentFunction->pushBack(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop.end"));
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
            loadAddress(addr,
                        type,
                        localUniqueName(expr.operation(), ".value"));
        auto const operation =
            expr.operation() == ast::UnaryPrefixOperator::Increment ?
                ir::ArithmeticOperation::Add :
                ir::ArithmeticOperation::Sub;
        auto* arithmetic =
            new ir::ArithmeticInst(value,
                                   irCtx.integralConstant(APInt(1, 64)),
                                   operation,
                                   localUniqueName(expr.operation(),
                                                   ".result"));
        currentBB()->pushBack(arithmetic);
        auto* store = new ir::Store(irCtx, addr, arithmetic);
        currentBB()->pushBack(store);
        return arithmetic;
    }
    ir::Value* const operand = getValue(*expr.operand);
    if (expr.operation() == ast::UnaryPrefixOperator::Promotion) {
        return operand;
    }
    auto* inst =
        new ir::UnaryArithmeticInst(irCtx,
                                    operand,
                                    mapUnaryArithmeticOp(expr.operation()),
                                    localUniqueName("expr.result"));
    currentBB()->pushBack(inst);
    return inst;
}

ir::Value* CodeGenContext::getValueImpl(BinaryExpression const& exprDecl) {
    switch (exprDecl.operation()) {
    case BinaryOperator::Multiplication: [[fallthrough]];
    case BinaryOperator::Division: [[fallthrough]];
    case BinaryOperator::Remainder: [[fallthrough]];
    case BinaryOperator::Addition: [[fallthrough]];
    case BinaryOperator::Subtraction: [[fallthrough]];
    case BinaryOperator::LeftShift: [[fallthrough]];
    case BinaryOperator::RightShift: [[fallthrough]];
    case BinaryOperator::BitwiseAnd: [[fallthrough]];
    case BinaryOperator::BitwiseXOr: [[fallthrough]];
    case BinaryOperator::BitwiseOr: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        ir::Value* const rhs = getValue(*exprDecl.rhs);
        auto* arithInst =
            new ir::ArithmeticInst(lhs,
                                   rhs,
                                   mapArithmeticOp(exprDecl.operation()),
                                   localUniqueName("expr.result"));
        currentBB()->pushBack(arithInst);
        return arithInst;
    }
    case BinaryOperator::LogicalAnd: [[fallthrough]];
    case BinaryOperator::LogicalOr: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        auto* startBlock     = currentBB();
        auto* rhsBlock =
            new ir::BasicBlock(irCtx, localUniqueName("logical.rhs"));
        auto* endBlock =
            new ir::BasicBlock(irCtx, localUniqueName("logical.end"));
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
                            localUniqueName("logical.and.value")) :
                new ir::Phi({ { startBlock, irCtx.integralConstant(1, 1) },
                              { rhsBlock, rhs } },
                            localUniqueName("logical.or.value"));
        currentBB()->pushBack(result);
        return result;
    }
    case BinaryOperator::Less: [[fallthrough]];
    case BinaryOperator::LessEq: [[fallthrough]];
    case BinaryOperator::Greater: [[fallthrough]];
    case BinaryOperator::GreaterEq: [[fallthrough]];
    case BinaryOperator::Equals: [[fallthrough]];
    case BinaryOperator::NotEquals: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        ir::Value* const rhs = getValue(*exprDecl.rhs);
        auto* cmpInst        = new ir::CompareInst(irCtx,
                                            lhs,
                                            rhs,
                                            mapCompareOp(exprDecl.operation()),
                                            localUniqueName("cmp.result"));
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
    case BinaryOperator::AddAssignment: [[fallthrough]];
    case BinaryOperator::SubAssignment: [[fallthrough]];
    case BinaryOperator::MulAssignment: [[fallthrough]];
    case BinaryOperator::DivAssignment: [[fallthrough]];
    case BinaryOperator::RemAssignment: [[fallthrough]];
    case BinaryOperator::LSAssignment: [[fallthrough]];
    case BinaryOperator::RSAssignment: [[fallthrough]];
    case BinaryOperator::AndAssignment: [[fallthrough]];
    case BinaryOperator::OrAssignment: [[fallthrough]];
    case BinaryOperator::XOrAssignment: {
        ir::Value* lhsAddr      = getAddress(*exprDecl.lhs);
        ir::Type const* lhsType = mapType(exprDecl.lhs->typeID());
        ir::Value* lhs          = loadAddress(lhsAddr, lhsType, "lhs");
        ir::Value* rhs          = getValue(*exprDecl.rhs);
        auto* result =
            new ir::ArithmeticInst(lhs,
                                   rhs,
                                   mapArithmeticAssignOp(exprDecl.operation()),
                                   localUniqueName("expr.result"));
        currentBB()->pushBack(result);
        auto* store = new ir::Store(irCtx, lhsAddr, result);
        currentBB()->pushBack(store);
        return loadAddress(lhsAddr, lhsType, "tmp");
    }
    case BinaryOperator::_count: SC_DEBUGFAIL();
    }
}

ir::Value* CodeGenContext::getValueImpl(MemberAccess const& expr) {
    return loadAddress(getAddressImpl(expr),
                       mapType(expr.typeID()),
                       "member.access");
}

ir::Value* CodeGenContext::getValueImpl(Conditional const& condExpr) {
    auto* cond = getValue(*condExpr.condition);
    auto* thenBlock =
        new ir::BasicBlock(irCtx, localUniqueName("conditional.then"));
    auto* elseBlock =
        new ir::BasicBlock(irCtx, localUniqueName("conditional.else"));
    auto* endBlock =
        new ir::BasicBlock(irCtx, localUniqueName("conditional.end"));
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
                    localUniqueName("conditional.result"));
    currentBB()->pushBack(result);
    return result;
}

ir::Value* CodeGenContext::getValueImpl(FunctionCall const& functionCall) {
    /// Handle calls to external functions separately.
    if (auto const& semaFunction =
            symTable.getFunction(functionCall.functionID());
        semaFunction.isExtern())
    {
        auto const args =
            functionCall.arguments |
            ranges::views::transform(
                [this](auto& expr) -> ir::Value* { return getValue(*expr); }) |
            ranges::to<utl::small_vector<ir::Value*>>;
        auto* call =
            new ir::ExtFunctionCall(semaFunction.slot(),
                                    semaFunction.index(),
                                    std::string(semaFunction.name()),
                                    args,
                                    mapType(semaFunction.signature()
                                                .returnTypeID()),
                                    functionCall.typeID() != symTable.Void() ?
                                        localUniqueName("call.result") :
                                        std::string{});
        currentBB()->pushBack(call);
        return call;
    }
    // TODO: Perform actual name mangling
    std::string const mangledName =
        utl::strcat(cast<Identifier const*>(functionCall.object.get())->value(),
                    functionCall.functionID());
    ir::Function* function = cast<ir::Function*>(irCtx.getGlobal(mangledName));
    auto const args =
        functionCall.arguments |
        ranges::views::transform(
            [this](auto& expr) -> ir::Value* { return getValue(*expr); }) |
        ranges::to<utl::small_vector<ir::Value*>>;
    auto* call = new ir::FunctionCall(function,
                                      args,
                                      functionCall.typeID() != symTable.Void() ?
                                          localUniqueName("call.result") :
                                          std::string{});
    currentBB()->pushBack(call);
    return call;
}

ir::Value* CodeGenContext::getValueImpl(Subscript const&) {
    SC_DEBUGFAIL();
}

ir::Value* CodeGenContext::getAddress(Expression const& expr) {
    return visit(expr,
                 [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* CodeGenContext::getAddressImpl(Identifier const& id) {
    auto itr = valueMap.find(id.symbolID());
    SC_ASSERT(itr != valueMap.end(), "Undeclared symbol");
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
        auto* addr =
            new ir::Alloca(irCtx, value->type(), localUniqueName("tmp"));
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
                                                localUniqueName("member.ptr"));
    currentBB()->pushBack(gep);
    return gep;
}

ir::Value* CodeGenContext::loadAddress(ir::Value* address,
                                ir::Type const* type,
                                std::string_view name) {
    auto* const load = new ir::Load(address, type, localUniqueName(name));
    currentBB()->pushBack(load);
    return load;
}

void CodeGenContext::declareTypes() {
    for (sema::TypeID const& typeID: symTable.sortedObjectTypes()) {
        auto const& objType   = symTable.getObjectType(typeID);
        auto structure = allocate<ir::StructureType>(
            mangledName(objType.symbolID(), objType.name()));
        for (sema::SymbolID const memberVarID: objType.memberVariables()) {
            auto& varDecl = symTable.getVariable(memberVarID);
            structure->addMember(mapType(varDecl.typeID()));
        }
        typeMap[typeID] = structure.get();
        mod.addStructure(std::move(structure));
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
        // TODO: Worry about name mangling
        auto* fn =
            new ir::Function(functionType,
                             mapType(function.signature().returnTypeID()),
                             paramTypes,
                             mangledName(function.symbolID(), function.name()));
        irCtx.addGlobal(fn);
    }
}

void CodeGenContext::setCurrentBB(ir::BasicBlock* bb) {
    _currentBB = bb;
}

void CodeGenContext::memorizeVariableAddress(sema::SymbolID symbolID,
                                      ir::Value* value) {
    [[maybe_unused]] auto const [_, insertSuccess] =
        valueMap.insert({ symbolID, value });
    SC_ASSERT(insertSuccess,
              "Variable id must not be declared multiple times. This error "
              "must be handled in sema.");
}

std::string CodeGenContext::localUniqueName(auto const&... args) {
    return irCtx.uniqueName(currentFunction, utl::strcat(args...));
}

std::string CodeGenContext::mangledName(sema::SymbolID id) const {
    return mangledName(id, symTable.getObjectType(id).name());
}

std::string CodeGenContext::mangledName(sema::SymbolID id,
                                 std::string_view name) const {
    return utl::format("{}{:x}", name, id.rawValue());
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
    if (auto itr = typeMap.find(semaTypeID);
        itr != typeMap.end())
    {
        return itr->second;
    }
    SC_DEBUGFAIL();
}

ir::UnaryArithmeticOperation CodeGenContext::mapUnaryArithmeticOp(
    ast::UnaryPrefixOperator op) {
    switch (op) {
    case UnaryPrefixOperator::Negation:
        return ir::UnaryArithmeticOperation::Negation;
    case UnaryPrefixOperator::BitwiseNot:
        return ir::UnaryArithmeticOperation::BitwiseNot;
    case UnaryPrefixOperator::LogicalNot:
        return ir::UnaryArithmeticOperation::LogicalNot;
    default: SC_UNREACHABLE();
    }
}

ir::CompareOperation CodeGenContext::mapCompareOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Less: return ir::CompareOperation::Less;
    case BinaryOperator::LessEq: return ir::CompareOperation::LessEq;
    case BinaryOperator::Greater: return ir::CompareOperation::Greater;
    case BinaryOperator::GreaterEq: return ir::CompareOperation::GreaterEq;
    case BinaryOperator::Equals: return ir::CompareOperation::Equal;
    case BinaryOperator::NotEquals: return ir::CompareOperation::NotEqual;
    default: SC_UNREACHABLE("Only handle compare operations here.");
    }
}

ir::ArithmeticOperation CodeGenContext::mapArithmeticOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Multiplication: return ir::ArithmeticOperation::Mul;
    case BinaryOperator::Division: return ir::ArithmeticOperation::Div;
    case BinaryOperator::Remainder: return ir::ArithmeticOperation::Rem;
    case BinaryOperator::Addition: return ir::ArithmeticOperation::Add;
    case BinaryOperator::Subtraction: return ir::ArithmeticOperation::Sub;
    case BinaryOperator::LeftShift: return ir::ArithmeticOperation::LShL;
    case BinaryOperator::RightShift: return ir::ArithmeticOperation::LShR;
    case BinaryOperator::BitwiseAnd: return ir::ArithmeticOperation::And;
    case BinaryOperator::BitwiseXOr: return ir::ArithmeticOperation::XOr;
    case BinaryOperator::BitwiseOr: return ir::ArithmeticOperation::Or;
    default: SC_UNREACHABLE("Only handle arithmetic operations here.");
    }
}

ir::ArithmeticOperation CodeGenContext::mapArithmeticAssignOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::AddAssignment: return ir::ArithmeticOperation::Add;
    case BinaryOperator::SubAssignment: return ir::ArithmeticOperation::Sub;
    case BinaryOperator::MulAssignment: return ir::ArithmeticOperation::Mul;
    case BinaryOperator::DivAssignment: return ir::ArithmeticOperation::Div;
    case BinaryOperator::RemAssignment: return ir::ArithmeticOperation::Rem;
    case BinaryOperator::LSAssignment: return ir::ArithmeticOperation::LShL;
    case BinaryOperator::RSAssignment: return ir::ArithmeticOperation::LShR;
    case BinaryOperator::AndAssignment: return ir::ArithmeticOperation::And;
    case BinaryOperator::OrAssignment: return ir::ArithmeticOperation::Or;
    case BinaryOperator::XOrAssignment: return ir::ArithmeticOperation::XOr;
    default: SC_UNREACHABLE("Only handle arithmetic assign operations here.");
    }
}
