#include "ASTCodeGen/CodeGen.h"

#include <utl/ranges.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "IR/Context.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    explicit Context(ir::Module& mod, ir::Context& irCtx, sema::SymbolTable const& symTable):
        mod(mod), irCtx(irCtx), symTable(symTable) {}

    ir::Value* dispatch(AbstractSyntaxTree const& node);

    ir::Value* generate(AbstractSyntaxTree const& node) { SC_UNREACHABLE(); }
    ir::Value* generate(TranslationUnit const&);
    ir::Value* generate(CompoundStatement const&);
    ir::Value* generate(FunctionDefinition const&);
    ir::Value* generate(StructDefinition const&);
    ir::Value* generate(VariableDeclaration const&);
    ir::Value* generate(ParameterDeclaration const&);
    ir::Value* generate(ExpressionStatement const&);
    ir::Value* generate(EmptyStatement const&);
    ir::Value* generate(ReturnStatement const&);
    ir::Value* generate(IfStatement const&);
    ir::Value* generate(WhileStatement const&);
    ir::Value* generate(DoWhileStatement const&);
    ir::Value* generate(Identifier const&);
    ir::Value* generate(IntegerLiteral const&);
    ir::Value* generate(BooleanLiteral const&);
    ir::Value* generate(FloatingPointLiteral const&);
    ir::Value* generate(StringLiteral const&);
    ir::Value* generate(UnaryPrefixExpression const&);
    ir::Value* generate(BinaryExpression const&);
    ir::Value* generate(MemberAccess const&);
    ir::Value* generate(Conditional const&);
    ir::Value* generate(FunctionCall const&);
    ir::Value* generate(Subscript const&);

    void declareFunctions();
    void setCurrentBB(ir::BasicBlock*);
    void finishCurrentBB();

    void memorizeVariablePtr(sema::SymbolID, ir::Value*);
    ir::Value* getVariablePtr(sema::SymbolID);

    std::string localUniqueName();

    ir::Type const* mapType(sema::TypeID semaTypeID);
    static ir::CompareOperation mapCompareOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticAssignOp(ast::BinaryOperator);

    ir::Module& mod;
    ir::Context& irCtx;
    sema::SymbolTable const& symTable;
    size_t varIndex               = 0;
    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* currentBB     = nullptr;
    utl::hashmap<sema::SymbolID, ir::Value*> valueMap;
};

} // namespace

ir::Module ast::codegen(AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable, ir::Context& context) {
    ir::Module mod;
    ir::Context irCtx;
    Context ctx(mod, irCtx, symbolTable);
    ctx.declareFunctions();
    ctx.dispatch(ast);
    return mod;
}

ir::Value* Context::dispatch(AbstractSyntaxTree const& node) {
    return visit(node, [this](auto const& node) { return generate(node); });
}

ir::Value* Context::generate(TranslationUnit const& tu) {
    for (auto& decl: tu.declarations) {
        auto* val = dispatch(*decl);
        if (isa<ir::Function>(val)) {
            mod.addFunction(cast<ir::Function*>(val));
        }
        else {
            SC_DEBUGFAIL();
        }
    }
    return nullptr;
}

ir::Value* Context::generate(CompoundStatement const& cmpStmt) {
    for (auto& statement: cmpStmt.statements) {
        dispatch(*statement);
    }
    return nullptr;
}

ir::Value* Context::generate(FunctionDefinition const& def) {
    utl::small_vector<ir::Type const*> paramTypes =
        utl::transform(def.parameters, [&](auto& param) { return mapType(param->typeID()); });
    // TODO: Also here worry about name mangling
    auto* fn    = cast<ir::Function*>(irCtx.getGlobal(utl::strcat(def.name(), def.symbolID())));
    varIndex    = def.parameters.size();
    auto* entry = new ir::BasicBlock(irCtx, localUniqueName());
    fn->addBasicBlock(entry);
    for (auto paramItr = fn->parameters().begin(); auto& paramDecl: def.parameters) {
        auto const* const irParamType = mapType(paramDecl->typeID());
        auto* paramMemPtr             = new ir::Alloca(irCtx, irParamType, localUniqueName());
        entry->addInstruction(paramMemPtr);
        memorizeVariablePtr(paramDecl->symbolID(), paramMemPtr);
        auto* store = new ir::Store(irCtx, paramMemPtr, std::to_address(paramItr++));
        entry->addInstruction(store);
    }
    currentFunction = fn;
    setCurrentBB(entry);
    dispatch(*def.body);
    setCurrentBB(nullptr);
    return fn;
}

ir::Value* Context::generate(StructDefinition const& def) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(VariableDeclaration const& varDecl) {
    auto* varMemPtr = new ir::Alloca(irCtx, mapType(varDecl.typeID()), localUniqueName());
    currentBB->addInstruction(varMemPtr);
    memorizeVariablePtr(varDecl.symbolID(), varMemPtr);
    if (varDecl.initExpression != nullptr) {
        ir::Value* initValue = dispatch(*varDecl.initExpression);
        auto* store          = new ir::Store(irCtx, varMemPtr, initValue);
        currentBB->addInstruction(store);
    }
    return varMemPtr;
}

ir::Value* Context::generate(ParameterDeclaration const&) {
    SC_UNREACHABLE("Handled by generate(FunctionDefinition const&)");
}

ir::Value* Context::generate(ExpressionStatement const& exprStatement) {
    dispatch(*exprStatement.expression);
    return nullptr;
}

ir::Value* Context::generate(EmptyStatement const& empty) {
    /// Nothing to do here.
    return nullptr;
}

ir::Value* Context::generate(ReturnStatement const& retDecl) {
    auto* returnValue = retDecl.expression ? dispatch(*retDecl.expression) : nullptr;
    auto* ret         = new ir::Return(irCtx, returnValue);
    currentBB->addInstruction(ret);
    return nullptr;
}

ir::Value* Context::generate(IfStatement const& ifStatement) {
    auto* condition = dispatch(*ifStatement.condition);
    auto* thenBlock = new ir::BasicBlock(irCtx, localUniqueName());
    auto* elseBlock = ifStatement.elseBlock ? new ir::BasicBlock(irCtx, localUniqueName()) : nullptr;
    auto* endBlock  = new ir::BasicBlock(irCtx, localUniqueName());
    auto* branch    = new ir::Branch(irCtx, condition, thenBlock, elseBlock ? elseBlock : endBlock);
    currentBB->addInstruction(branch);
    auto addBlock = [&](ir::BasicBlock* bb, ast::Statement const& block) {
        currentFunction->addBasicBlock(bb);
        setCurrentBB(bb);
        dispatch(block);
        auto* gotoEnd = new ir::Goto(irCtx, endBlock);
        currentBB->addInstruction(gotoEnd);
    };
    addBlock(thenBlock, *ifStatement.ifBlock);
    if (ifStatement.elseBlock) {
        addBlock(elseBlock, *ifStatement.elseBlock);
    }
    currentFunction->addBasicBlock(endBlock);
    setCurrentBB(endBlock);
    return nullptr;
}

ir::Value* Context::generate(WhileStatement const& loopDecl) {
    auto* loopHeader = new ir::BasicBlock(irCtx, localUniqueName());
    currentFunction->addBasicBlock(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName());
    currentFunction->addBasicBlock(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName());
    currentFunction->addBasicBlock(loopEnd);
    auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
    currentBB->addInstruction(gotoLoopHeader);
    setCurrentBB(loopHeader);
    auto* condition = dispatch(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB->addInstruction(branch);
    currentBB = loopBody;
    dispatch(*loopDecl.block);
    auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
    currentBB->addInstruction(gotoLoopHeader2);
    setCurrentBB(loopEnd);
    return nullptr;
}

ir::Value* Context::generate(DoWhileStatement const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(Identifier const& id) {
    auto* memPtr   = getVariablePtr(id.symbolID());
    auto* loadInst = new ir::Load(mapType(id.typeID()), memPtr, localUniqueName());
    currentBB->addInstruction(loadInst);
    return loadInst;
}

ir::Value* Context::generate(IntegerLiteral const& intLit) {
    return irCtx.integralConstant(intLit.value(), 64);
}

ir::Value* Context::generate(BooleanLiteral const& boolLit) {
    return irCtx.integralConstant(boolLit.value(), 1);
}

ir::Value* Context::generate(FloatingPointLiteral const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(StringLiteral const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(UnaryPrefixExpression const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(BinaryExpression const& exprDecl) {
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
        ir::Value* const lhs = dispatch(*exprDecl.lhs);
        ir::Value* const rhs = dispatch(*exprDecl.rhs);
        auto* arithInst = new ir::ArithmeticInst(lhs, rhs, mapArithmeticOp(exprDecl.operation()), localUniqueName());
        currentBB->addInstruction(arithInst);
        return arithInst;
    }
    case BinaryOperator::LogicalAnd: [[fallthrough]];
    case BinaryOperator::LogicalOr: {
        ir::Value* const lhs = dispatch(*exprDecl.lhs);
        auto* startBlock     = currentBB;
        auto* rhsBlock       = new ir::BasicBlock(irCtx, localUniqueName());
        auto* endBlock       = new ir::BasicBlock(irCtx, localUniqueName());
        currentBB->addInstruction(exprDecl.operation() == BinaryOperator::LogicalAnd ?
                                      new ir::Branch(irCtx, lhs, rhsBlock, endBlock) :
                                      new ir::Branch(irCtx, lhs, endBlock, rhsBlock));
        currentFunction->addBasicBlock(rhsBlock);
        setCurrentBB(rhsBlock);
        auto* rhs = dispatch(*exprDecl.rhs);
        currentBB->addInstruction(new ir::Goto(irCtx, endBlock));
        currentFunction->addBasicBlock(endBlock);
        setCurrentBB(endBlock);
        auto* result = exprDecl.operation() == BinaryOperator::LogicalAnd ?
                           new ir::Phi(irCtx.integralType(1),
                                       { { startBlock, irCtx.integralConstant(0, 1) }, { rhsBlock, rhs } },
                                       localUniqueName()) :
                           new ir::Phi(irCtx.integralType(1),
                                       { { startBlock, irCtx.integralConstant(1, 1) }, { rhsBlock, rhs } },
                                       localUniqueName());
        currentBB->addInstruction(result);
        return result;
    }
    case BinaryOperator::Less: [[fallthrough]];
    case BinaryOperator::LessEq: [[fallthrough]];
    case BinaryOperator::Greater: [[fallthrough]];
    case BinaryOperator::GreaterEq: [[fallthrough]];
    case BinaryOperator::Equals: [[fallthrough]];
    case BinaryOperator::NotEquals: {
        ir::Value* const lhs = dispatch(*exprDecl.lhs);
        ir::Value* const rhs = dispatch(*exprDecl.rhs);
        auto* cmpInst = new ir::CompareInst(irCtx, lhs, rhs, mapCompareOp(exprDecl.operation()), localUniqueName());
        currentBB->addInstruction(cmpInst);
        return cmpInst;
    }
    case BinaryOperator::Comma: {
        dispatch(*exprDecl.lhs);
        return dispatch(*exprDecl.rhs);
    }
    case BinaryOperator::Assignment: [[fallthrough]];
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
        ir::Value* const lhs = exprDecl.operation() != BinaryOperator::Assignment ? dispatch(*exprDecl.lhs) : nullptr;
        ir::Value* const rhs = dispatch(*exprDecl.rhs);
        auto* lhsPointer     = getVariablePtr(cast<Identifier const*>(exprDecl.lhs.get())->symbolID());
        ir::Value* value     = [&]() -> ir::Value* {
            if (exprDecl.operation() == BinaryOperator::Assignment) {
                return rhs;
            }
            auto* arithInst =
                new ir::ArithmeticInst(lhs, rhs, mapArithmeticAssignOp(exprDecl.operation()), localUniqueName());
            currentBB->addInstruction(arithInst);
            return arithInst;
        }();
        auto* store = new ir::Store(irCtx, lhsPointer, value);
        currentBB->addInstruction(store);
        // TODO: Maybe return value or memory address here.
        return nullptr;
    }
    case BinaryOperator::_count: SC_DEBUGFAIL();
    }
}

ir::Value* Context::generate(MemberAccess const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::generate(Conditional const& condExpr) {
    ir::Type const* type = mapType(condExpr.typeID());
    auto* cond           = dispatch(*condExpr.condition);
    auto* thenBlock      = new ir::BasicBlock(irCtx, localUniqueName());
    auto* elseBlock      = new ir::BasicBlock(irCtx, localUniqueName());
    auto* endBlock       = new ir::BasicBlock(irCtx, localUniqueName());
    currentBB->addInstruction(new ir::Branch(irCtx, cond, thenBlock, elseBlock));
    currentFunction->addBasicBlock(thenBlock);
    setCurrentBB(thenBlock);
    auto* thenVal = dispatch(*condExpr.ifExpr);
    currentBB->addInstruction(new ir::Goto(irCtx, endBlock));
    currentFunction->addBasicBlock(elseBlock);
    setCurrentBB(elseBlock);
    auto* elseVal = dispatch(*condExpr.elseExpr);
    currentBB->addInstruction(new ir::Goto(irCtx, endBlock));
    currentFunction->addBasicBlock(endBlock);
    setCurrentBB(endBlock);
    auto* result = new ir::Phi(type, { { thenBlock, thenVal }, { elseBlock, elseVal } }, localUniqueName());
    currentBB->addInstruction(result);
    return result;
}

ir::Value* Context::generate(FunctionCall const& functionCall) {
    // TODO: Perform actual name mangling
    std::string const mangledName =
        utl::strcat(cast<Identifier const*>(functionCall.object.get())->value(), functionCall.functionID());
    ir::Function* function = cast<ir::Function*>(irCtx.getGlobal(mangledName));
    utl::small_vector<ir::Value*> const args =
        utl::transform(functionCall.arguments, [this](auto& expr) -> ir::Value* { return dispatch(*expr); });
    auto* call = new ir::FunctionCall(function,
                                      args,
                                      functionCall.typeID() != symTable.Void() ? localUniqueName() : std::string{});
    currentBB->addInstruction(call);
    return call;
}

ir::Value* Context::generate(Subscript const&) {
    SC_DEBUGFAIL();
}

void Context::declareFunctions() {
    for (sema::Function const& function: symTable.functions()) {
        utl::small_vector<ir::Type const*> paramTypes =
            utl::transform(function.signature().argumentTypeIDs(),
                           [&](sema::TypeID paramTypeID) { return mapType(paramTypeID); });
        // TODO: Generate proper function type here
        ir::FunctionType const* const functionType = nullptr;
        // TODO: Worry about name mangling
        auto* fn = new ir::Function(functionType,
                                    mapType(function.signature().returnTypeID()),
                                    paramTypes,
                                    utl::strcat(function.name(), function.symbolID()));
        irCtx.addGlobal(fn);
    }
}

void Context::setCurrentBB(ir::BasicBlock* bb) {
    finishCurrentBB();
    currentBB = bb;
}

void Context::finishCurrentBB() {
    if (currentBB == nullptr) {
        return;
    }
    auto& instructions = currentBB->instructions;
    for (auto itr = instructions.begin(); itr != instructions.end(); ++itr) {
        auto& inst = *itr;
        if (!isa<ir::TerminatorInst>(inst)) {
            continue;
        }
        instructions.erase(std::next(itr), instructions.end());
        break;
    }
}

void Context::memorizeVariablePtr(sema::SymbolID symbolID, ir::Value* value) {
    [[maybe_unused]] auto const [_, insertSuccess] = valueMap.insert({ symbolID, value });
    SC_ASSERT(insertSuccess, "Variable must not be declared multiple times. This error should be handled in sema.");
}

ir::Value* Context::getVariablePtr(sema::SymbolID symbolID) {
    auto itr = valueMap.find(symbolID);
    SC_ASSERT(itr != valueMap.end(), "Undeclared symbol");
    return itr->second;
}

std::string Context::localUniqueName() {
    return std::to_string(varIndex++);
}

ir::Type const* Context::mapType(sema::TypeID semaTypeID) {
    if (semaTypeID == symTable.Void()) {
        return irCtx.voidType();
    }
    else if (semaTypeID == symTable.Int()) {
        return irCtx.integralType(64);
    }
    else if (semaTypeID == symTable.Bool()) {
        return irCtx.integralType(1);
    }
    else {
        SC_DEBUGFAIL();
    }
}

ir::CompareOperation Context::mapCompareOp(ast::BinaryOperator op) {
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

ir::ArithmeticOperation Context::mapArithmeticOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::Multiplication: return ir::ArithmeticOperation::Mul;
    case BinaryOperator::Division: return ir::ArithmeticOperation::Div;
    case BinaryOperator::Remainder: return ir::ArithmeticOperation::Rem;
    case BinaryOperator::Addition: return ir::ArithmeticOperation::Add;
    case BinaryOperator::Subtraction: return ir::ArithmeticOperation::Sub;
    case BinaryOperator::LeftShift: return ir::ArithmeticOperation::ShiftL;
    case BinaryOperator::RightShift: return ir::ArithmeticOperation::ShiftR;
    case BinaryOperator::BitwiseAnd: return ir::ArithmeticOperation::And;
    case BinaryOperator::BitwiseXOr: return ir::ArithmeticOperation::Or;
    case BinaryOperator::BitwiseOr: return ir::ArithmeticOperation::XOr;
    default: SC_UNREACHABLE("Only handle arithmetic operations here.");
    }
}

ir::ArithmeticOperation Context::mapArithmeticAssignOp(ast::BinaryOperator op) {
    switch (op) {
    case BinaryOperator::AddAssignment: return ir::ArithmeticOperation::Add;
    case BinaryOperator::SubAssignment: return ir::ArithmeticOperation::Sub;
    case BinaryOperator::MulAssignment: return ir::ArithmeticOperation::Mul;
    case BinaryOperator::DivAssignment: return ir::ArithmeticOperation::Div;
    case BinaryOperator::RemAssignment: return ir::ArithmeticOperation::Rem;
    case BinaryOperator::LSAssignment: return ir::ArithmeticOperation::ShiftL;
    case BinaryOperator::RSAssignment: return ir::ArithmeticOperation::ShiftR;
    case BinaryOperator::AndAssignment: return ir::ArithmeticOperation::And;
    case BinaryOperator::OrAssignment: return ir::ArithmeticOperation::Or;
    case BinaryOperator::XOrAssignment: return ir::ArithmeticOperation::XOr;
    default: SC_UNREACHABLE("Only handle arithmetic assign operations here.");
    }
}
