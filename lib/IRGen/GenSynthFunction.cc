#include "IRGen/FunctionGeneration.h"

#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

namespace {

struct Loop {
    ir::BasicBlock* body;
    ir::Value* index;
    ir::Instruction const* insertPoint;
};

struct FuncGenContext: FuncGenContextBase {
    sema::ObjectType const* parentType;
    sema::SpecialLifetimeFunction kind;

    FuncGenContext(auto&... args):
        FuncGenContextBase(args...),
        parentType(cast<sema::ObjectType const*>(semaFn.parent())),
        kind(semaFn.SLFKind()) {}

    void generate();

    void genImpl(sema::StructType const& type);
    void genImpl(sema::ArrayType const& type);
    void genImpl(sema::UniquePtrType const& type);
    void genImpl(sema::ObjectType const& type) { SC_UNREACHABLE(); }

    void genMemberCall(ir::Instruction const* before,
                       sema::ObjectType const& memberType,
                       ir::Value* index);

    ///
    utl::small_vector<ir::Value*, 2> genArguments(ir::Instruction const* before,
                                                  ir::Type const* inType,
                                                  ir::Value* index);

    /// \Returns the loop structure
    Loop genLoop(size_t count);
};

} // namespace

utl::small_vector<sema::Function const*> irgen::generateSynthFunction(
    sema::Function const& semaFn,
    ir::Function& irFn,
    ir::Context& ctx,
    ir::Module& mod,
    sema::SymbolTable const& symbolTable,
    TypeMap const& typeMap,
    FunctionMap& functionMap) {
    SC_ASSERT(semaFn.isSpecialLifetimeFunction(),
              "We only generate special lifetime functions here");
    utl::small_vector<sema::Function const*> declaredFunctions;
    FuncGenContext synthContext(semaFn,
                                irFn,
                                ctx,
                                mod,
                                symbolTable,
                                typeMap,
                                functionMap,
                                declaredFunctions);
    synthContext.generate();
    ir::setupInvariants(ctx, irFn);
    ir::assertInvariants(ctx, irFn);
    return declaredFunctions;
}

void FuncGenContext::generate() {
    addNewBlock("entry");
    visit(*parentType, [&](auto& type) { genImpl(type); });
}

void FuncGenContext::genImpl(sema::StructType const& type) {
    for (auto* var: type.memberVariables()) {
        /// `cast` is safe here because data member must be of object type
        genMemberCall(currentBlock().end().to_address(),
                      *cast<sema::ObjectType const*>(var->type()),
                      ctx.intConstant(var->index(), 64));
    }
}

void FuncGenContext::genImpl(sema::ArrayType const& type) {
    SC_ASSERT(!type.isDynamic(),
              "Cannot generate SLF for dynamic array (or can we?)");
    auto* elemType = type.elementType();
    SC_ASSERT(!elemType->hasTrivialLifetime(),
              "We should not generate lifetime functions for arrays of trivial "
              "lifetime types");
    auto loop = genLoop(type.count());
    if (!loop.body) {
        return;
    }
    withBlockCurrent(loop.body, [&] {
        genMemberCall(loop.insertPoint, *elemType, loop.index);
    });
}

void FuncGenContext::genImpl(sema::UniquePtrType const& type) {
    using enum sema::SpecialLifetimeFunction;
    switch (kind) {
    case DefaultConstructor: {
        auto* self = &irFn.parameters().front();
        add<ir::Store>(self, ctx.nullpointer());
        break;
    }
    case CopyConstructor:
        SC_UNREACHABLE();
    case MoveConstructor: {
        auto* self = &irFn.parameters().front();
        auto* rhs = &irFn.parameters().back();
        add<ir::Store>(self, add<ir::Load>(rhs, ctx.ptrType(), "rhs"));
        add<ir::Store>(rhs, ctx.nullpointer());
        break;
    }
    case Destructor: {
        auto* self = &irFn.parameters().front();
        auto* ptr = add<ir::Load>(self, ctx.ptrType(), "ptr");
        auto* cond = add<ir::CompareInst>(ptr,
                                          ctx.nullpointer(),
                                          ir::CompareMode::Unsigned,
                                          ir::CompareOperation::Equal,
                                          "ptr.null");
        auto* then = newBlock("delete");
        auto* end = newBlock("end");
        add<ir::Branch>(cond, end, then);

        add(then);
        auto* baseType = type.base().get();
        if (auto* dtor = baseType->specialLifetimeFunction(Destructor)) {
            add<ir::Call>(getFunction(dtor), std::array<ir::Value*, 1>{ ptr });
        }
        auto* dealloc = getBuiltin(svm::Builtin::dealloc);
        std::array<ir::Value*, 3> args = {
            ptr,
            ctx.intConstant(type.base()->size(), 64),
            ctx.intConstant(type.base()->align(), 64)
        };
        add<ir::Call>(dealloc, args);
        add<ir::Goto>(end);

        add(end);
        break;
    }
    }
}

void FuncGenContext::genMemberCall(ir::Instruction const* before,
                                   sema::ObjectType const& type,
                                   ir::Value* index) {
    auto arguments = genArguments(before, typeMap(&type), index);
    if (auto* f = type.specialLifetimeFunction(kind)) {
        insert<ir::Call>(before, getFunction(f), arguments);
        return;
    }
    SC_ASSERT(type.hasTrivialLifetime(),
              "This function cannot be generated if the member type does not "
              "support the operation");
    using enum sema::SpecialLifetimeFunction;
    switch (kind) {
    case DefaultConstructor:
        /// TODO: Zero the memory here
        break;
    case MoveConstructor:
        [[fallthrough]];
    case CopyConstructor: {
        auto* irType = typeMap(&type);
        auto* value = insert<ir::Load>(before, arguments[1], irType, "value");
        insert<ir::Store>(before, arguments[0], value);
        break;
    }
    case Destructor:
        break;
    }
}

utl::small_vector<ir::Value*, 2> FuncGenContext::genArguments(
    ir::Instruction const* before, ir::Type const* inType, ir::Value* index) {
    return irFn.parameters() | TakeAddress |
           ranges::views::transform([&](auto* param) {
               return insert<ir::GetElementPointer>(before,
                                                    inType,
                                                    param,
                                                    index,
                                                    std::array<size_t, 0>{},
                                                    "mem.acc");
           }) |
           ranges::to<utl::small_vector<ir::Value*, 2>>;
}

Loop FuncGenContext::genLoop(size_t count) {
    if (count == 0) {
        return {};
    }
    auto* pred = &currentBlock();
    auto* body = newBlock("loop.body");
    auto* end = newBlock("loop.end");

    add<ir::Goto>(body);
    add(body);

    auto* phi =
        add<ir::Phi>(std::array{ ir::PhiMapping{ pred, ctx.intConstant(0, 64) },
                                 ir::PhiMapping{ body, nullptr } },
                     "counter");
    auto* inc = add<ir::ArithmeticInst>(phi,
                                        ctx.intConstant(1, 64),
                                        ir::ArithmeticOperation::Add,
                                        "loop.inc");
    phi->setArgument(1, inc);
    auto* cond = add<ir::CompareInst>(inc,
                                      ctx.intConstant(count, 64),
                                      ir::CompareMode::Unsigned,
                                      ir::CompareOperation::Equal,
                                      "loop.test");
    add<ir::Branch>(cond, end, body);
    add(end);

    return Loop{ body, phi, inc };
}
