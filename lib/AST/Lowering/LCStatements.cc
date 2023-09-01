#include "AST/Lowering/LoweringContext.h"

#include <svm/Builtin.h>
#include <utl/scope_guard.hpp>

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;
using sema::QualType;

using enum ValueLocation;

void LoweringContext::generate(AbstractSyntaxTree const& node) {
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

void LoweringContext::generateParameter(
    ParameterDeclaration const* paramDecl,
    PassingConvention pc,
    List<ir::Parameter>::iterator& irParamItr) {
    QualType semaType = paramDecl->type();
    auto* irParam = irParamItr.to_address();
    auto* irType = mapType(paramDecl->type());
    std::string name(paramDecl->name());
    /// The `this` reference parameter is not stored to memory
    if (auto* thisParam = dyncast<ThisParameter const*>(paramDecl);
        thisParam && sema::isRef(thisParam->type()))
    {
        SC_ASSERT(pc.location() == Register, "");
        memorizeObject(paramDecl->variable(),
                       Value(newID(), irParam, Register));
        ++irParamItr;
        return;
    }

    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(stripReference(semaType).get()))
    {
        switch (pc.location()) {
        case Register: {
            auto* dataAddress = storeLocal(irParam, name);
            auto* sizeAddress =
                storeLocal(irParam->next(), utl::strcat(name, ".size"));
            Value data(newID(), dataAddress, irType, Memory);
            Value size(newID(), sizeAddress, irParam->next()->type(), Memory);
            memorizeObject(paramDecl->variable(), data);
            memorizeArraySize(data.ID(), size);
            break;
        }

        case Memory: {
            Value data(newID(), irParam, irType, Memory);
            Value size(newID(), irParam->next(), Register);
            memorizeObject(paramDecl->variable(), data);
            memorizeArraySize(data.ID(), size);
            break;
        }
        }
        std::advance(irParamItr, 2);
    }
    else {
        switch (pc.location()) {
        case Register: {
            auto* address = storeLocal(irParam, name);
            memorizeObject(paramDecl->variable(),
                           Value(newID(), address, irType, Memory));
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
    for (auto* statement:
         def.body()->statements() | ranges::views::filter([](auto* statement) {
             return isa<FunctionDefinition>(statement);
         }))
    {
        generate(*statement);
    }
}

void LoweringContext::generateImpl(VariableDeclaration const& varDecl) {
    auto dtorStack = varDecl.dtorStack();
    utl::scope_guard emitDTors = [&] { emitDestructorCalls(dtorStack); };
    std::string name = utl::strcat(varDecl.name());
    auto* arrayType = dyncast<sema::ArrayType const*>(
        sema::stripReference(varDecl.type()).get());
    /// Simple non-array case
    if (!arrayType) {
        if (varDecl.initExpression()) {
            auto value = getValue(varDecl.initExpression());
            if (value.isRValue()) {
                value.get()->setName(name);
                memorizeObject(varDecl.variable(), value.toLValue());
                if (!dtorStack.empty() &&
                    dtorStack.top().object ==
                        varDecl.initExpression()->entity())
                {
                    dtorStack.pop();
                }
                return;
            }
            auto* address = storeLocal(toRegister(value), name);
            memorizeObject(varDecl.variable(),
                           Value(newID(), address, value.type(), Memory));
            return;
        }
        auto* type = mapType(varDecl.type());
        auto* address = makeLocal(type, name);
        memorizeObject(varDecl.variable(),
                       Value(newID(), address, type, Memory));
        return;
    }
    /// Array case
    if (sema::isRef(varDecl.type())) {
        auto data = getValue(varDecl.initExpression());
        auto* dataAddress =
            storeLocal(toRegister(data), std::string(varDecl.name()));
        auto const ID = newID();
        memorizeObject(varDecl.variable(),
                       Value(ID, dataAddress, data.get()->type(), Memory));
        auto count = getArraySize(data.ID());
        auto* sizeAddress =
            storeLocal(toRegister(count), utl::strcat(varDecl.name(), ".size"));
        memorizeArraySize(ID,
                          Value(newID(), sizeAddress, count.type(), Memory));
        return;
    }
    SC_ASSERT(!arrayType->isDynamic(), "Can't locally allocate dynamic array");
    /// We can steal the data from an rvalue
    if (varDecl.initExpression() && varDecl.initExpression()->isRValue()) {
        auto data = getValue(varDecl.initExpression());
        SC_ASSERT(data.isRValue(), "");
        data.get()->setName(name);
        memorizeObject(varDecl.variable(), data);
        return;
    }
    /// We need to allocate our own data
    auto* elemType = mapType(arrayType->elementType());
    auto* array = new ir::Alloca(ctx,
                                 intConstant(arrayType->count(), 32),
                                 elemType,
                                 name);
    allocas.push_back(array);
    if (varDecl.initExpression()) {
        auto data = getValue(varDecl.initExpression());
        auto* memcpy = getFunction(symbolTable.builtinFunction(
            static_cast<size_t>(svm::Builtin::memcpy)));
        auto* size = intConstant(arrayType->count() * elemType->size(), 64);
        std::array<ir::Value*, 4> args = { array, size, data.get(), size };
        add<ir::Call>(memcpy, args);
    }
    auto data = Value(newID(), array, mapType(arrayType), Memory);
    memorizeObject(varDecl.variable(), data);
    memorizeArraySize(data.ID(), arrayType->count());
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
    switch (sema::stripReference(retDecl.expression()->type())->entityType()) {
    case sema::EntityType::ArrayType: {
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
        break;
    }
    default:
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
        break;
    }
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
