#include "AST/Lowering/LoweringContext.h"

#include "AST/AST.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace ast;

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
}

void LoweringContext::generateImpl(FunctionDefinition const& def) {
    currentFunction =
        cast<ir::Function*>(functionMap.find(def.function())->second);
    auto* entry = currentBlock = addNewBlock("entry");

    for (auto [paramDecl, irParam]:
         ranges::views::zip(def.parameters(), currentFunction->parameters()))
    {
        auto* address = storeLocal(&irParam, std::string(paramDecl->name()));
        memorizeVariableAddress(paramDecl->entity(), address);
    }

    generate(*def.body());
    currentBlock    = nullptr;
    currentFunction = nullptr;

    /// Add all generated `alloca`s to the entry basic block
    auto itr = std::next(entry->begin());
    for (auto* allocaInst: allocas) {
        entry->insert(itr, allocaInst);
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
    if (auto* arrayType =
            dyncast<sema::ArrayType const*>(varDecl.type()->base()))
    {
        /// Array case
        SC_DEBUGFAIL();
        //        auto* address = [&]() -> ir::Value* {
        //            if (varDecl.initExpression()) {
        //                if (varDecl.initExpression()->isRValue()) {
        //                    return getAddress(*varDecl.initExpression());
        //                }
        //                else {
        //                    // TODO: Copy the array
        //                    SC_DEBUGFAIL();
        //                }
        //            }
        //            auto* elemType = mapType(arrayType->elementType());
        //            auto* count =
        //            ctx.integralConstant(APInt(arrayType->count(), 32)); auto*
        //            array = new ir::Alloca(ctx,
        //                                         count,
        //                                         elemType,
        //                                         utl::strcat(varDecl.name(),
        //                                         ".addr"));
        //            addAlloca(array);
        //            return array;
        //        }();
        //        /// Stupid hack: We make an array view object with our address
        //        and our
        //        /// size and store that to memory. Then this array works just
        //        like an
        //        /// array view
        //        auto* count     =
        //        ctx.integralConstant(APInt(arrayType->count(), 64)); auto*
        //        arrayView = makeArrayView(address, count); auto* viewAddr  =
        //        storeTmpToMemory(arrayView);
        //        memorizeVariableAddress(varDecl.variable(), viewAddr);
        //        return;
    }
    /// Non-array case

    if (varDecl.initExpression()) {
        auto* value   = getValue(varDecl.initExpression());
        auto* address = storeLocal(value, std::string(varDecl.name()));
        memorizeVariableAddress(varDecl.entity(), address);
    }
    else {
        auto* type    = mapType(varDecl.type());
        auto* address = makeLocal(type, std::string(varDecl.name()));
        memorizeVariableAddress(varDecl.entity(), address);
    }
}

void LoweringContext::generateImpl(ParameterDeclaration const&) {
    SC_UNREACHABLE("Handled by generate(FunctionDefinition)");
}

void LoweringContext::generateImpl(ExpressionStatement const& exprStatement) {
    (void)getValue(exprStatement.expression());
}

void LoweringContext::generateImpl(ReturnStatement const& retDecl) {
    auto* returnValue =
        retDecl.expression() ? getValue(retDecl.expression()) : ctx.voidValue();
    add<ir::Return>(returnValue);
}

void LoweringContext::generateImpl(IfStatement const& stmt) {
    auto* condition = getValue(stmt.condition());
    auto* thenBlock = newBlock("if.then");
    auto* elseBlock = stmt.elseBlock() ? newBlock("if.else") : nullptr;
    auto* endBlock  = newBlock("if.end");
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
        auto* loopBody   = newBlock("loop.body");
        auto* loopInc    = newBlock("loop.inc");
        auto* loopEnd    = newBlock("loop.end");
        generate(*loopStmt.varDecl());
        add<ir::Goto>(loopHeader);

        /// Header
        add(loopHeader);
        auto* condition = getValue(loopStmt.condition());
        add<ir::Branch>(condition, loopBody, loopEnd);
        currentLoop = { .header = loopHeader,
                        .body   = loopBody,
                        .inc    = loopInc,
                        .end    = loopEnd };

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopInc);

        /// Inc
        add(loopInc);
        getValue(loopStmt.increment());
        add<ir::Goto>(loopHeader);

        /// End
        add(loopEnd);
        currentLoop = {};
        break;
    }

    case ast::LoopKind::While: {
        auto* loopHeader = newBlock("loop.header");
        auto* loopBody   = newBlock("loop.body");
        auto* loopEnd    = newBlock("loop.end");
        add<ir::Goto>(loopHeader);

        /// Header
        add(loopHeader);
        auto* condition = getValue(loopStmt.condition());
        add<ir::Branch>(condition, loopBody, loopEnd);
        currentLoop = { .header = loopHeader,
                        .body   = loopBody,
                        .end    = loopEnd };

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopHeader);

        /// End
        add(loopEnd);
        currentLoop = {};
        break;
    }

    case ast::LoopKind::DoWhile: {
        auto* loopBody   = newBlock("loop.body");
        auto* loopFooter = newBlock("loop.footer");
        auto* loopEnd    = newBlock("loop.end");
        add<ir::Goto>(loopBody);
        currentLoop = { .header = loopFooter,
                        .body   = loopBody,
                        .end    = loopEnd };

        /// Body
        add(loopBody);
        generate(*loopStmt.block());
        add<ir::Goto>(loopFooter);

        /// Footer
        add(loopFooter);
        auto* condition = getValue(loopStmt.condition());
        add<ir::Branch>(condition, loopBody, loopEnd);

        /// End
        add(loopEnd);
        currentLoop = {};
        break;
    }
    }
}

void LoweringContext::generateImpl(JumpStatement const& jump) {
    auto* dest = [&] {
        switch (jump.kind()) {
        case JumpStatement::Break:
            return currentLoop.end;
        case JumpStatement::Continue:
            return currentLoop.inc ? currentLoop.inc : currentLoop.header;
        }
    }();
    add<ir::Goto>(dest);
}
