#include "IRGen/FunctionGeneration.h"

#include "IR/Builder.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

namespace {

struct FuncGenContext: FuncGenContextBase {
    sema::CompoundType const* parentType;
    sema::SpecialLifetimeFunction kind;

    FuncGenContext(auto&... args):
        FuncGenContextBase(args...),
        parentType(cast<sema::CompoundType const*>(semaFn.parent())),
        kind(semaFn.SLFKind()) {}

    void generate();

    void genImpl(sema::StructType const& type);
    void genImpl(sema::ArrayType const& type);

    void genMemberCall(sema::ObjectType const& memberType, ir::Value* index);

    ///
    utl::small_vector<ir::Value*, 2> genArguments(ir::Type const* inType,
                                                  ir::Value* index);

    /// \Returns the loop body and the loop index
    std::pair<ir::BasicBlock*, ir::Value*> loop(size_t count);
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
    auto* parentType = cast<sema::CompoundType const*>(semaFn.parent());
    visit(*parentType, [&](auto& type) { genImpl(type); });
}

void FuncGenContext::genImpl(sema::StructType const& type) {
    for (auto* var: type.memberVariables()) {
        /// `cast` is safe here because data member must be of object type
        genMemberCall(*cast<sema::ObjectType const*>(var->type()),
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
    auto [body, index] = loop(type.count());
    if (!body) {
        return;
    }
    withBlockCurrent(body,
                     [&, index = index] { genMemberCall(*elemType, index); });
}

void FuncGenContext::genMemberCall(sema::ObjectType const& type,
                                   ir::Value* index) {
    auto arguments = genArguments(typeMap(&type), index);
    if (auto* compType = dyncast<sema::CompoundType const*>(&type)) {
        if (auto* f = compType->specialLifetimeFunction(kind)) {
            add<ir::Call>(getFunction(f), arguments);
            return;
        }
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
        auto* value = add<ir::Load>(arguments[1], irType, "value");
        add<ir::Store>(arguments[0], value);
        break;
    }
    case Destructor:
        break;
    }
}

utl::small_vector<ir::Value*, 2> FuncGenContext::genArguments(
    ir::Type const* inType, ir::Value* index) {
    return irFn.parameters() | TakeAddress |
           ranges::views::transform([&](auto* param) {
               return add<ir::GetElementPointer>(inType,
                                                 param,
                                                 index,
                                                 std::array<size_t, 0>{},
                                                 "mem.acc");
           }) |
           ranges::to<utl::small_vector<ir::Value*, 2>>;
}

std::pair<ir::BasicBlock*, ir::Value*> FuncGenContext::loop(size_t count) {
    if (count == 0) {
        return {};
    }
    auto* body = newBlock("loop.body");
    auto* footer = newBlock("loop.footer");
    auto* end = newBlock("loop.end");

    add(body);
    auto* phi =
        add<ir::Phi>(std::array{ ir::PhiMapping{ &currentBlock(),
                                                 ctx.intConstant(0, 64) },
                                 ir::PhiMapping{ &currentBlock(), nullptr } },
                     "counter");
    add<ir::Goto>(footer);
    add(footer);
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
    return { body, phi };
}
