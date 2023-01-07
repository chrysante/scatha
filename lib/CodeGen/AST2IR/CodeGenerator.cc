#include "CodeGen/AST2IR/CodeGenerator.h"

#include <utl/format.hpp>
#include <utl/ranges.hpp>
#include <utl/stack.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

namespace {

struct Context {
    explicit Context(ir::Module& mod, ir::Context& irCtx, sema::SymbolTable const& symTable):
        mod(mod), irCtx(irCtx), symTable(symTable) {}

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
    
    ir::Value* getValueImpl(AbstractSyntaxTree const& expr) { SC_UNREACHABLE(); } // Delete this later
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

    ir::Value* getAddressImpl(AbstractSyntaxTree const& expr) { SC_UNREACHABLE(); }
    ir::Value* getAddressImpl(Expression const& expr) { SC_UNREACHABLE(); }
    ir::Value* getAddressImpl(Identifier const&);
    ir::Value* getAddressImpl(MemberAccess const&);
    
    ir::Value* loadAddress(ir::Value* address, std::string_view name);
    
    void declareTypes();
    void declareFunctions();
    
    ir::BasicBlock* currentBB() { return _currentBB; }
    void setCurrentBB(ir::BasicBlock*);
    void _finishCurrentBB();

    void memorizeVariableAddress(sema::SymbolID, ir::Value*);

    std::string localUniqueName(std::string_view name);
    std::string localUniqueName(std::string_view name, std::convertible_to<std::string_view> auto&&... args);

    std::string mangledName(sema::SymbolID) const;
    std::string mangledName(sema::SymbolID, std::string_view name) const;
    ir::Type const* mapType(sema::TypeID semaTypeID);
    static ir::UnaryArithmeticOperation mapUnaryArithmeticOp(ast::UnaryPrefixOperator);
    static ir::CompareOperation mapCompareOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticOp(ast::BinaryOperator);
    static ir::ArithmeticOperation mapArithmeticAssignOp(ast::BinaryOperator);

    ir::Module& mod;
    ir::Context& irCtx;
    sema::SymbolTable const& symTable;
    ir::Function* currentFunction = nullptr;
    ir::BasicBlock* _currentBB    = nullptr;
    utl::hashmap<sema::SymbolID, ir::Value*> valueMap;
    // For unique names
    utl::hashmap<std::string, size_t> varIndices;
};

} // namespace

ir::Module ast::codegen(AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable, ir::Context& context) {
    ir::Module mod;
    ir::Context irCtx;
    Context ctx(mod, irCtx, symbolTable);
    ctx.declareTypes();
    ctx.declareFunctions();
    ctx.generate(ast);
    return mod;
}

void Context::generate(AbstractSyntaxTree const& node) {
    visit(node, [this](auto const& node) { return generateImpl(node); });
}

void Context::generateImpl(TranslationUnit const& tu) {
    for (auto& decl: tu.declarations) {
        generate(*decl);
    }
}

void Context::generateImpl(CompoundStatement const& cmpStmt) {
    for (auto& statement: cmpStmt.statements) {
        generate(*statement);
    }
}

void Context::generateImpl(FunctionDefinition const& def) {
    utl::small_vector<ir::Type const*> paramTypes =
        utl::transform(def.parameters, [&](auto& param) { return mapType(param->typeID()); });
    // TODO: Also here worry about name mangling
    auto* fn    = cast<ir::Function*>(irCtx.getGlobal(utl::strcat(def.name(), def.symbolID())));
    auto* entry = new ir::BasicBlock(irCtx, localUniqueName("entry"));
    fn->addBasicBlock(entry);
    for (auto paramItr = fn->parameters().begin(); auto& paramDecl: def.parameters) {
        auto const* const irParamType = mapType(paramDecl->typeID());
        auto* paramMemPtr             = new ir::Alloca(irCtx, irParamType, localUniqueName(paramDecl->name(), "-ptr"));
        entry->addInstruction(paramMemPtr);
        memorizeVariableAddress(paramDecl->symbolID(), paramMemPtr);
        auto* store = new ir::Store(irCtx, paramMemPtr, std::to_address(paramItr++));
        entry->addInstruction(store);
    }
    currentFunction = fn;
    setCurrentBB(entry);
    generate(*def.body);
    setCurrentBB(nullptr);
    currentFunction = nullptr;
    varIndices.clear();
    mod.addFunction(fn);
}

void Context::generateImpl(StructDefinition const& def) {
    /// Nothing to do here, structs are handled separately.
}

void Context::generateImpl(VariableDeclaration const& varDecl) {
    auto* varMemPtr = new ir::Alloca(irCtx, mapType(varDecl.typeID()), localUniqueName(varDecl.name(), "-ptr"));
    currentBB()->addInstruction(varMemPtr);
    memorizeVariableAddress(varDecl.symbolID(), varMemPtr);
    if (varDecl.initExpression == nullptr) {
        return;
    }
    ir::Value* initValue = getValue(*varDecl.initExpression);
    auto* store = new ir::Store(irCtx, varMemPtr, initValue);
    currentBB()->addInstruction(store);
}

void Context::generateImpl(ParameterDeclaration const&) {
    SC_UNREACHABLE("Handled by generate(FunctionDefinition)");
}

void Context::generateImpl(ExpressionStatement const& exprStatement) {
    (void)getValue(*exprStatement.expression);
}

void Context::generateImpl(EmptyStatement const& empty) {
    /// Nothing to do here.
}

void Context::generateImpl(ReturnStatement const& retDecl) {
    auto* returnValue = retDecl.expression ? getValue(*retDecl.expression) : nullptr;
    auto* ret         = new ir::Return(irCtx, returnValue);
    currentBB()->addInstruction(ret);
}

void Context::generateImpl(IfStatement const& ifStatement) {
    auto* condition = getValue(*ifStatement.condition);
    auto* thenBlock = new ir::BasicBlock(irCtx, localUniqueName("then-block"));
    auto* elseBlock = ifStatement.elseBlock ? new ir::BasicBlock(irCtx, localUniqueName("else-block")) : nullptr;
    auto* endBlock  = new ir::BasicBlock(irCtx, localUniqueName("if-end"));
    auto* branch    = new ir::Branch(irCtx, condition, thenBlock, elseBlock ? elseBlock : endBlock);
    currentBB()->addInstruction(branch);
    auto addBlock = [&](ir::BasicBlock* bb, ast::Statement const& block) {
        currentFunction->addBasicBlock(bb);
        setCurrentBB(bb);
        generate(block);
        auto* gotoEnd = new ir::Goto(irCtx, endBlock);
        currentBB()->addInstruction(gotoEnd);
    };
    addBlock(thenBlock, *ifStatement.ifBlock);
    if (ifStatement.elseBlock) {
        addBlock(elseBlock, *ifStatement.elseBlock);
    }
    currentFunction->addBasicBlock(endBlock);
    setCurrentBB(endBlock);
}

void Context::generateImpl(WhileStatement const& loopDecl) {
    auto* loopHeader = new ir::BasicBlock(irCtx, localUniqueName("loop-header"));
    currentFunction->addBasicBlock(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop-body"));
    currentFunction->addBasicBlock(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop-end"));
    currentFunction->addBasicBlock(loopEnd);
    auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
    currentBB()->addInstruction(gotoLoopHeader);
    setCurrentBB(loopHeader);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->addInstruction(branch);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
    currentBB()->addInstruction(gotoLoopHeader2);
    setCurrentBB(loopEnd);
}

void Context::generateImpl(DoWhileStatement const& loopDecl) {
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop-body"));
    currentFunction->addBasicBlock(loopBody);
    auto* loopFooter = new ir::BasicBlock(irCtx, localUniqueName("loop-footer"));
    currentFunction->addBasicBlock(loopFooter);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop-end"));
    currentFunction->addBasicBlock(loopEnd);
    auto* gotoLoopBody = new ir::Goto(irCtx, loopBody);
    currentBB()->addInstruction(gotoLoopBody);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    auto* gotoLoopFooter = new ir::Goto(irCtx, loopFooter);
    currentBB()->addInstruction(gotoLoopFooter);
    setCurrentBB(loopFooter);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->addInstruction(branch);
    setCurrentBB(loopEnd);
}

void Context::generateImpl(ForStatement const& loopDecl) {
    auto* loopPreheader = new ir::BasicBlock(irCtx, localUniqueName("loop-preheader"));
    currentFunction->addBasicBlock(loopPreheader);
    auto* loopHeader = new ir::BasicBlock(irCtx, localUniqueName("loop-header"));
    currentFunction->addBasicBlock(loopHeader);
    auto* loopBody = new ir::BasicBlock(irCtx, localUniqueName("loop-body"));
    currentFunction->addBasicBlock(loopBody);
    auto* loopEnd = new ir::BasicBlock(irCtx, localUniqueName("loop-end"));
    currentFunction->addBasicBlock(loopEnd);
    auto* gotoLoopPreheader = new ir::Goto(irCtx, loopPreheader);
    currentBB()->addInstruction(gotoLoopPreheader);
    setCurrentBB(loopPreheader);
    generate(*loopDecl.varDecl);
    auto* gotoLoopHeader = new ir::Goto(irCtx, loopHeader);
    currentBB()->addInstruction(gotoLoopHeader);
    setCurrentBB(loopHeader);
    auto* condition = getValue(*loopDecl.condition);
    auto* branch    = new ir::Branch(irCtx, condition, loopBody, loopEnd);
    currentBB()->addInstruction(branch);
    setCurrentBB(loopBody);
    generate(*loopDecl.block);
    getValue(*loopDecl.increment);
    auto* gotoLoopHeader2 = new ir::Goto(irCtx, loopHeader);
    currentBB()->addInstruction(gotoLoopHeader2);
    setCurrentBB(loopEnd);
}

ir::Value* Context::getValue(Expression const& expr) {
    return visit(expr, [this](auto const& expr) { return getValueImpl(expr); });
}

ir::Value* Context::getValueImpl(Identifier const& id) {
    return loadAddress(getAddressImpl(id), id.value());
}

ir::Value* Context::getValueImpl(IntegerLiteral const& intLit) {
    return irCtx.integralConstant(intLit.value(), 64);
}

ir::Value* Context::getValueImpl(BooleanLiteral const& boolLit) {
    return irCtx.integralConstant(boolLit.value(), 1);
}

ir::Value* Context::getValueImpl(FloatingPointLiteral const& floatLit) {
    return irCtx.floatConstant(floatLit.value(), 64);
}

ir::Value* Context::getValueImpl(StringLiteral const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::getValueImpl(UnaryPrefixExpression const& expr) {
    ir::Value* const operand = getValue(*expr.operand);
    auto* inst = new ir::UnaryArithmeticInst(irCtx, operand, mapUnaryArithmeticOp(expr.operation()), "expr-result");
    currentBB()->addInstruction(inst);
    return inst;
}

ir::Value* Context::getValueImpl(BinaryExpression const& exprDecl) {
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
            new ir::ArithmeticInst(lhs, rhs, mapArithmeticOp(exprDecl.operation()), localUniqueName("expr-result"));
        currentBB()->addInstruction(arithInst);
        return arithInst;
    }
    case BinaryOperator::LogicalAnd: [[fallthrough]];
    case BinaryOperator::LogicalOr: {
        ir::Value* const lhs = getValue(*exprDecl.lhs);
        auto* startBlock     = currentBB();
        auto* rhsBlock       = new ir::BasicBlock(irCtx, localUniqueName("logical-rhs-block"));
        auto* endBlock       = new ir::BasicBlock(irCtx, localUniqueName("logical-end-block"));
        currentBB()->addInstruction(exprDecl.operation() == BinaryOperator::LogicalAnd ?
                                      new ir::Branch(irCtx, lhs, rhsBlock, endBlock) :
                                      new ir::Branch(irCtx, lhs, endBlock, rhsBlock));
        currentFunction->addBasicBlock(rhsBlock);
        setCurrentBB(rhsBlock);
        auto* rhs = getValue(*exprDecl.rhs);
        currentBB()->addInstruction(new ir::Goto(irCtx, endBlock));
        currentFunction->addBasicBlock(endBlock);
        setCurrentBB(endBlock);
        auto* result = exprDecl.operation() == BinaryOperator::LogicalAnd ?
                           new ir::Phi({ { startBlock, irCtx.integralConstant(0, 1) }, { rhsBlock, rhs } },
                                       localUniqueName("logical-and-value")) :
                           new ir::Phi({ { startBlock, irCtx.integralConstant(1, 1) }, { rhsBlock, rhs } },
                                       localUniqueName("logical-or-value"));
        currentBB()->addInstruction(result);
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
        auto* cmpInst =
            new ir::CompareInst(irCtx, lhs, rhs, mapCompareOp(exprDecl.operation()), localUniqueName("cmp-result"));
        currentBB()->addInstruction(cmpInst);
        return cmpInst;
    }
    case BinaryOperator::Comma: {
        getValue(*exprDecl.lhs);
        return getValue(*exprDecl.rhs);
    }
    case BinaryOperator::Assignment: {
        ir::Value* const lhsAddr = getAddress(*exprDecl.lhs);
        ir::Value* const rhs     = getValue(*exprDecl.rhs);
        auto* store = new ir::Store(irCtx, lhsAddr, rhs);
        currentBB()->addInstruction(store);
        return loadAddress(lhsAddr, "tmp");
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
        ir::Value* const lhsAddr = getAddress(*exprDecl.lhs);
        ir::Value* const lhs     = loadAddress(lhsAddr, "lhs-value");
        ir::Value* const rhs     = getValue(*exprDecl.rhs);
        auto* result = new ir::ArithmeticInst(lhs,
                                              rhs,
                                              mapArithmeticAssignOp(exprDecl.operation()),
                                              localUniqueName("expr-result"));
        currentBB()->addInstruction(result);
        auto* store = new ir::Store(irCtx, lhsAddr, result);
        currentBB()->addInstruction(store);
        return loadAddress(lhsAddr, "tmp");
    }
    case BinaryOperator::_count: SC_DEBUGFAIL();
    }
}

ir::Value* Context::getValueImpl(MemberAccess const& expr) {
    return loadAddress(getAddressImpl(expr), "member-access");
}

ir::Value* Context::getValueImpl(Conditional const& condExpr) {
    auto* cond           = getValue(*condExpr.condition);
    auto* thenBlock      = new ir::BasicBlock(irCtx, localUniqueName("then-block"));
    auto* elseBlock      = new ir::BasicBlock(irCtx, localUniqueName("else-block"));
    auto* endBlock       = new ir::BasicBlock(irCtx, localUniqueName("conditional-end"));
    currentBB()->addInstruction(new ir::Branch(irCtx, cond, thenBlock, elseBlock));
    currentFunction->addBasicBlock(thenBlock);
    /// Generate then block.
    setCurrentBB(thenBlock);
    auto* thenVal = getValue(*condExpr.ifExpr);
    thenBlock     = currentBB();
    thenBlock->addInstruction(new ir::Goto(irCtx, endBlock));
    currentFunction->addBasicBlock(elseBlock);
    /// Generate else block.
    setCurrentBB(elseBlock);
    auto* elseVal = getValue(*condExpr.elseExpr);
    elseBlock     = currentBB();
    elseBlock->addInstruction(new ir::Goto(irCtx, endBlock));
    currentFunction->addBasicBlock(endBlock);
    /// Generate end block.
    setCurrentBB(endBlock);
    auto* result =
        new ir::Phi({ { thenBlock, thenVal }, { elseBlock, elseVal } }, localUniqueName("conditional-result"));
    currentBB()->addInstruction(result);
    return result;
}

ir::Value* Context::getValueImpl(FunctionCall const& functionCall) {
    /// Handle calls to external functions separately.
    if (auto const& semaFunction = symTable.getFunction(functionCall.functionID()); semaFunction.isExtern()) {
        utl::small_vector<ir::Value*> const args =
            utl::transform(functionCall.arguments, [this](auto& expr) -> ir::Value* { return getValue(*expr); });
        auto* call =
            new ir::ExtFunctionCall(semaFunction.slot(),
                                    semaFunction.index(),
                                    args,
                                    mapType(semaFunction.signature().returnTypeID()),
                                    functionCall.typeID() != symTable.Void() ? localUniqueName("ext-call-result") :
                                                                               std::string{});
        currentBB()->addInstruction(call);
        return call;
    }
    // TODO: Perform actual name mangling
    std::string const mangledName =
        utl::strcat(cast<Identifier const*>(functionCall.object.get())->value(), functionCall.functionID());
    ir::Function* function = cast<ir::Function*>(irCtx.getGlobal(mangledName));
    utl::small_vector<ir::Value*> const args =
        utl::transform(functionCall.arguments, [this](auto& expr) -> ir::Value* { return getValue(*expr); });
    auto* call =
        new ir::FunctionCall(function,
                             args,
                             functionCall.typeID() != symTable.Void() ? localUniqueName("call-result") : std::string{});
    currentBB()->addInstruction(call);
    return call;
}

ir::Value* Context::getValueImpl(Subscript const&) {
    SC_DEBUGFAIL();
}

ir::Value* Context::getAddress(Expression const& expr) {
    return visit(expr, [this](auto const& expr) { return getAddressImpl(expr); });
}

ir::Value* Context::getAddressImpl(Identifier const& id) {
    auto itr = valueMap.find(id.symbolID());
    SC_ASSERT(itr != valueMap.end(), "Undeclared symbol");
    return itr->second;
}

ir::Value* Context::getAddressImpl(MemberAccess const& expr) {
    /// Get the value or the address based on wether the base object is an l-value or r-value
    ir::Value* basePtr = [&]() -> ir::Value* {
        if (expr.object->valueCategory() == ast::ValueCategory::LValue) {
            return getAddress(*expr.object);
        }
        /// If we are an r-value we store the value to memory and return a pointer to it.
        auto* value = getValue(*expr.object);
        auto* addr = new ir::Alloca(irCtx, value->type(), "tmp-ptr");
        currentBB()->addInstruction(addr);
        auto* store = new ir::Store(irCtx, addr, value);
        currentBB()->addInstruction(store);
        return addr;
    }();
    sema::SymbolID const accessedElementID = cast<Identifier const&>(*expr.member).symbolID();
    auto& var                              = symTable.getVariable(accessedElementID);
    size_t const index                     = var.index();
    ir::Type const* const accessedType     = mapType(expr.object->typeID());
    auto* const gep = new ir::GetElementPointer(irCtx, accessedType, basePtr, index, localUniqueName("member-ptr"));
    currentBB()->addInstruction(gep);
    return gep;
}

ir::Value* Context::loadAddress(ir::Value* address, std::string_view name) {
    auto* const load = new ir::Load(address, localUniqueName(name));
    currentBB()->addInstruction(load);
    return load;
}

void Context::declareTypes() {
    for (sema::TypeID const& typeID: symTable.sortedObjectTypes()) {
        auto const& objType   = symTable.getObjectType(typeID);
        auto* const structure = new ir::StructureType(mangledName(objType.symbolID(), objType.name()));
        for (sema::SymbolID const memberVarID: objType.memberVariables()) {
            auto& varDecl = symTable.getVariable(memberVarID);
            structure->addMember(mapType(varDecl.typeID()));
        }
        mod.addStructure(structure);
    }
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
                                    mangledName(function.symbolID(), function.name()));
        irCtx.addGlobal(fn);
    }
}

void Context::setCurrentBB(ir::BasicBlock* bb) {
    _finishCurrentBB();
    _currentBB = bb;
}

void Context::_finishCurrentBB() {
    if (currentBB() == nullptr) {
        return;
    }
    auto& instructions = currentBB()->instructions;
    for (auto itr = instructions.begin(); itr != instructions.end(); ++itr) {
        auto& inst = *itr;
        if (!isa<ir::TerminatorInst>(inst)) {
            continue;
        }
        instructions.erase(std::next(itr), instructions.end());
        break;
    }
    if (instructions.empty() || !isa<ir::TerminatorInst>(instructions.back())) {
        /// Issue returns to non-terminating basic blocks. This should correspond to void functions with implicit return
        /// statements.
        instructions.push_back(new ir::Return(irCtx));
    }
}

void Context::memorizeVariableAddress(sema::SymbolID symbolID, ir::Value* value) {
    [[maybe_unused]] auto const [_, insertSuccess] = valueMap.insert({ symbolID, value });
    SC_ASSERT(insertSuccess, "Variable id must not be declared multiple times. This error must be handled in sema.");
}

std::string Context::localUniqueName(std::string_view name) {
    auto itr = varIndices.find(name);
    if (itr == varIndices.end()) {
        varIndices.insert({ std::string(name), 1 });
        return std::string(name);
    }
    return utl::strcat(name, "-", itr->second++);
}

std::string Context::localUniqueName(std::string_view name, std::convertible_to<std::string_view> auto&&... args) {
    return localUniqueName(utl::strcat(name, std::forward<decltype(args)>(args)...));
}

std::string Context::mangledName(sema::SymbolID id) const {
    return mangledName(id, symTable.getObjectType(id).name());
}

std::string Context::mangledName(sema::SymbolID id, std::string_view name) const {
    return utl::format("{}{:x}", name, id.rawValue());
}

ir::Type const* Context::mapType(sema::TypeID semaTypeID) {
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
    std::string const name = mangledName(semaTypeID);
    auto itr               = mod.structures().find(std::string_view(name));
    if (itr != mod.structures().end()) {
        return *itr;
    }
    SC_DEBUGFAIL();
}

ir::UnaryArithmeticOperation Context::mapUnaryArithmeticOp(ast::UnaryPrefixOperator op) {
    switch (op) {
    case UnaryPrefixOperator::Promotion: return ir::UnaryArithmeticOperation::Promotion;
    case UnaryPrefixOperator::Negation: return ir::UnaryArithmeticOperation::Negation;
    case UnaryPrefixOperator::BitwiseNot: return ir::UnaryArithmeticOperation::BitwiseNot;
    case UnaryPrefixOperator::LogicalNot: return ir::UnaryArithmeticOperation::LogicalNot;
    default: SC_UNREACHABLE();
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
    case BinaryOperator::BitwiseXOr: return ir::ArithmeticOperation::XOr;
    case BinaryOperator::BitwiseOr: return ir::ArithmeticOperation::Or;
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
