#include "IRGen/LoweringContext.h"

#include <svm/Builtin.h>
#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;
using sema::QualType;

using enum ValueLocation;

void LoweringContext::generate(ASTNode const& node) {
    visit(node, [this](auto const& node) { return generateImpl(node); });
}

void LoweringContext::generateImpl(TranslationUnit const& tu) {
    for (auto* decl: tu.declarations()) {
        generate(*decl);
    }
}

void LoweringContext::generateImpl(CompoundStatement const& cmpStmt) {
    for (auto* statement: cmpStmt.statements()) {
        generate(*statement);
    }
    emitDestructorCalls(cmpStmt.dtorStack());
}

void LoweringContext::generateImpl(FunctionDefinition const& def) {
    currentSemaFunction = def.function();
    currentFunction =
        cast<ir::Function*>(functionMap.find(def.function())->second);
    auto* entry = currentBlock = addNewBlock("entry");
    auto CC = CCMap[currentSemaFunction];
    auto irParamItr = currentFunction->parameters().begin();
    if (CC.returnValue().location() == Memory) {
        ++irParamItr;
    }
    for (auto [paramDecl, pc]:
         ranges::views::zip(def.parameters(), CC.arguments()))
    {
        generateParameter(paramDecl, pc, irParamItr);
    }
    generate(*def.body());
    currentBlock = nullptr;
    currentFunction = nullptr;
    currentSemaFunction = nullptr;
    /// Add all generated `alloca`s to the entry basic block
    for (auto before = entry->begin(); auto* allocaInst: allocas) {
        entry->insert(before, allocaInst);
    }
    allocas.clear();
}

sema::ObjectType const* stripPtrAndRef(sema::ObjectType const* type) {
    if (auto* ptrType = dyncast<sema::PointerType const*>(type)) {
        return ptrType->base().get();
    }
    if (auto* refType = dyncast<sema::ReferenceType const*>(type)) {
        return refType->base().get();
    }
    return type;
}

void LoweringContext::generateParameter(
    ParameterDeclaration const* paramDecl,
    PassingConvention pc,
    List<ir::Parameter>::iterator& irParamItr) {
    QualType semaType = paramDecl->type();
    auto* irParam = irParamItr.to_address();
    auto* irType = mapType(paramDecl->type());
    std::string name(paramDecl->name());

    auto* arrayType =
        dyncast<sema::ArrayType const*>(stripPtrAndRef(semaType.get()));
    if (arrayType) {
        switch (pc.location()) {
        case Register: {
            if (isa<sema::ReferenceType>(*paramDecl->type())) {
                Value data(newID(), irParam, Register);
                memorizeObject(paramDecl->variable(), data);
                ++irParamItr;
                if (arrayType->isDynamic()) {
                    Value size(newID(), irParam->next(), Register);
                    memorizeArraySize(data.ID(), size);
                    ++irParamItr;
                }
            }
            else if (isa<sema::PointerType>(*paramDecl->type())) {
                Value data(newID(),
                           storeLocal(irParam),
                           irParam->type(),
                           Memory);
                memorizeObject(paramDecl->variable(), data);
                ++irParamItr;
                if (arrayType->isDynamic()) {
                    Value size(newID(),
                               storeLocal(irParam->next()),
                               irParam->next()->type(),
                               Memory);
                    memorizeArraySize(data.ID(), size);
                    ++irParamItr;
                }
            }
            else {
                SC_ASSERT(!arrayType->isDynamic(),
                          "Can't pass dynamic array by value");
                auto* dataAddress = storeLocal(irParam, name);
                auto* sizeValue = ctx.integralConstant(arrayType->count(), 64);
                Value data(newID(), dataAddress, irType, Memory);
                Value size(newID(), sizeValue, Register);
                memorizeObject(paramDecl->variable(), data);
                memorizeArraySize(data.ID(), size);
                ++irParamItr;
            }
            break;
        }

        case Memory: {
            Value data(newID(), irParam, irType, Memory);
            Value size(newID(), irParam->next(), Register);
            memorizeObject(paramDecl->variable(), data);
            memorizeArraySize(data.ID(), size);
            std::advance(irParamItr, 2);
            break;
        }
        }
    }
    else {
        switch (pc.location()) {
        case Register: {
            if (isa<sema::ReferenceType>(*paramDecl->type())) {
                memorizeObject(paramDecl->variable(),
                               Value(newID(), irParam, Register));
            }
            else {
                auto* address = storeLocal(irParam, name);
                memorizeObject(paramDecl->variable(),
                               Value(newID(), address, irType, Memory));
            }
            break;
        }

        case Memory: {
            memorizeObject(paramDecl->variable(),
                           Value(newID(), irParam, irType, Memory));
            break;
        }
        }
        ++irParamItr;
    }
}

void LoweringContext::generateImpl(StructDefinition const& def) {
    for (auto* stmt: def.body()->statements() | Filter<FunctionDefinition>) {
        generate(*stmt);
    }
}

void LoweringContext::generateArraySizeImpl(
    ast::VarDeclBase const* varDecl,
    uint32_t varID,
    utl::function_view<ir::Value*()> sizeCallback) {
    auto* type = stripPtrAndRef(varDecl->type().get());
    auto* arrayType = dyncast<sema::ArrayType const*>(type);
    if (!arrayType) {
        return;
    }
    if (!arrayType->isDynamic()) {
        memorizeArraySize(varID, arrayType->count());
        return;
    }
    if (sema::isRef(varDecl->type())) {
        memorizeArraySize(varID, Value(newID(), sizeCallback(), Register));
    }
    else {
        auto* size = sizeCallback();
        auto* sizeVar = storeLocal(size, utl::strcat(varDecl->name(), ".size"));
        memorizeArraySize(varID, Value(newID(), sizeVar, size->type(), Memory));
    }
}

void LoweringContext::generateVarDeclArraySize(ast::VarDeclBase const* varDecl,
                                               uint32_t varID,
                                               uint32_t initID) {
    generateArraySizeImpl(varDecl, varID, [&] {
        return toRegister(getArraySize(initID));
    });
}

void LoweringContext::generateParamArraySize(ast::VarDeclBase const* varDecl,
                                             uint32_t varID,
                                             ir::Parameter* param) {
    generateArraySizeImpl(varDecl, varID, [&] { return param->next(); });
}

static bool varDeclNeedCopy(QualType type) {
    return type->hasTrivialLifetime() && !isa<sema::ArrayType>(*type);
}

void LoweringContext::generateImpl(VariableDeclaration const& varDecl) {
    auto dtorStack = varDecl.dtorStack();
    std::string name = std::string(varDecl.name());
    auto* initExpr = varDecl.initExpression();
    if (sema::isRef(varDecl.type())) {
        SC_ASSERT(initExpr, "Reference must be initialized");
        auto value = getValue(initExpr);
        memorizeObject(varDecl.variable(), value);
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
            address = storeLocal(toRegister(value), name);
        }
        auto varID = newID();
        memorizeObject(varDecl.variable(),
                       Value(varID, address, value.type(), Memory));
        generateVarDeclArraySize(&varDecl, varID, value.ID());
    }
    else {
        auto* type = mapType(varDecl.type());
        auto* address = makeLocal(type, name);
        auto varID = newID();
        memorizeObject(varDecl.variable(), Value(varID, address, type, Memory));
        generateVarDeclArraySize(&varDecl, varID, uint32_t(-1));
    }
    emitDestructorCalls(dtorStack);
}

void LoweringContext::generateImpl(ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
    emitDestructorCalls(exprStatement.dtorStack());
}

void LoweringContext::generateImpl(ReturnStatement const& retDecl) {
    auto CC = CCMap[currentSemaFunction];
    if (!retDecl.expression()) {
        add<ir::Return>(ctx.voidValue());
        return;
    }
    auto returnValue = getValue(retDecl.expression());
    emitDestructorCalls(retDecl.dtorStack());
    // clang-format off
    SC_MATCH (*stripRefOrPtr(retDecl.expression()->type())) {
        [&](sema::ObjectType const&) {
            switch (CC.returnValue().location()) {
            case Register:
                add<ir::Return>(toRegister(returnValue));
                break;
                
            case Memory:
                add<ir::Store>(&currentFunction->parameters().front(),
                               toRegister(returnValue));
                add<ir::Return>(ctx.voidValue());
                break;
            }
        },
        [&](sema::ArrayType const&) {
            switch (CC.returnValue().location()) {
            case Register: {
                auto* data = toRegister(returnValue);
                auto* size = toRegister(getArraySize(returnValue.ID()));
                auto* insertData = add<ir::InsertValue>(ctx.undef(arrayViewType),
                                                        data,
                                                        std::array{ size_t{ 0 } },
                                                        "retval");
                auto* insertSize = add<ir::InsertValue>(insertData,
                                                        size,
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

void LoweringContext::generateImpl(IfStatement const& stmt) {
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

void LoweringContext::generateImpl(LoopStatement const& loopStmt) {
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

void LoweringContext::generateImpl(JumpStatement const& jump) {
    emitDestructorCalls(jump.dtorStack());
    auto* dest = [&] {
        auto& currentLoop = loopStack.top();
        switch (jump.kind()) {
        case JumpStatement::Break:
            return currentLoop.end;
        case JumpStatement::Continue:
            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
        }
    }();
    add<ir::Goto>(dest);
}
