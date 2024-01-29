#include "IRGen/FunctionGeneration.h"

#include "IR/Builder.h"
#include "IR/CFG/Constants.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/Instructions.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/Utility.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"

using namespace scatha;
using namespace irgen;

namespace {

struct FuncGenContext: FuncGenContextBase {
    sema::ObjectType const* parentType;
    sema::SMFKind kind;

    FuncGenContext(sema::SMFKind kind, auto&... args):
        FuncGenContextBase(args...),
        parentType(cast<sema::ObjectType const*>(semaFn.parent())),
        kind(kind) {}

    void generate();

    void genImpl(sema::StructType const& type);
    void genImpl(sema::ArrayType const& type);
    ir::Value* getUniquePtrCountAddr(ir::Value* thisPtr);
    void genImpl(sema::ObjectType const&) { SC_UNREACHABLE(); }

    void genMemberConstruction(ir::BasicBlock::ConstIterator before,
                               sema::ObjectType const& memberType,
                               ir::Value* index);

    ///
    utl::small_vector<ir::Value*, 2> genArguments(ir::Instruction const* before,
                                                  ir::Type const* inType,
                                                  ir::Value* index);
};

} // namespace

// void irgen::generateSynthFunction(Config config, FuncGenParameters params) {
//     generateSynthFunctionAs(params.semaFn.smfKind().value(), config, params);
// }

// void irgen::generateSynthFunctionAs(sema::SMFKind kind, Config config,
//                                     FuncGenParameters params) {
//     FuncGenContext synthContext(kind, config, params);
//     synthContext.generate();
// }

void FuncGenContext::generate() {
    addNewBlock("entry");
    visit(*parentType, [&](auto& type) { genImpl(type); });
}

void FuncGenContext::genImpl(sema::StructType const& type) {
    for (auto* var: type.memberVariables()) {
        /// `cast` is safe here because data member must be of object type
        genMemberConstruction(currentBlock().end(),
                              *cast<sema::ObjectType const*>(var->type()),
                              ctx.intConstant(var->index(), 64));
    }
}

void FuncGenContext::genImpl(sema::ArrayType const& type) {
    auto* elemType = type.elementType();
    auto* count = [&]() -> ir::Value* {
        if (type.isDynamic()) {
            /// The count is the second function argument
            return irFn.parameters().front().next();
        }
        else {
            return ctx.intConstant(type.count(), 64);
        }
    }();
    auto loop = generateForLoop("arraylifetime", count);
    withBlockCurrent(loop.body, [&] {
        genMemberConstruction(loop.insertPoint, *elemType, loop.induction);
    });
}

ir::Value* FuncGenContext::getUniquePtrCountAddr(ir::Value* thisPtr) {
    return add<ir::GetElementPointer>(ctx, arrayPtrType, thisPtr, nullptr,
                                      std::array{ size_t{ 1 } }, "sizeptr");
}

void FuncGenContext::genMemberConstruction(ir::BasicBlock::ConstIterator before,
                                           sema::ObjectType const& type,
                                           ir::Value* index) {
    //    auto arguments = genArguments(before, typeMap(&type), index);
    //    /// Non-trivial case
    //    if (auto* f = type.specialLifetimeFunction(kind)) {
    //        insert<ir::Call>(before, getFunction(f), arguments);
    //        return;
    //    }
    //    SC_ASSERT(type.hasTrivialLifetime(),
    //              "This function cannot be generated if the member type does
    //              not " "support the operation");
    //    /// Trivial case
    //    using enum sema::SpecialLifetimeFunctionDepr;
    //    switch (kind) {
    //    case DefaultConstructor: {
    //        auto* memset = getBuiltin(svm::Builtin::memset);
    //        ir::Value* irZero = ctx.intConstant(0, 64);
    //        std::array<ir::Value*, 3> args = { arguments[0],
    //                                           ctx.intConstant(type.size(),
    //                                           64), irZero };
    //        insert<ir::Call>(before, memset, args, std::string{});
    //        break;
    //    }
    //    case MoveConstructor:
    //        [[fallthrough]];
    //    case CopyConstructor: {
    //        auto* irType = typeMap(&type);
    //        auto* value = insert<ir::Load>(before, arguments[1], irType,
    //        "value"); insert<ir::Store>(before, arguments[0], value); break;
    //    }
    //    case Destructor:
    //        break;
    //    }
}

static size_t SMFKindNumPtrParams(sema::SMFKind kind) {
    using enum sema::SMFKind;
    switch (kind) {
    case DefaultConstructor:
        return 1;
    case CopyConstructor:
        return 2;
    case MoveConstructor:
        return 2;
    case Destructor:
        return 1;
    }
    SC_UNREACHABLE();
}

utl::small_vector<ir::Value*, 2> FuncGenContext::genArguments(
    ir::Instruction const* before, ir::Type const* inType, ir::Value* index) {
    utl::small_vector<ir::Value*, 2> args;
    size_t numParams = SMFKindNumPtrParams(kind);
    auto& params = irFn.parameters();
    auto* parentType = cast<sema::Type const*>(semaFn.parent());
    int stride = isDynArray(*parentType) ? 2 : 1;
    size_t loopIndex = 0;
    for (auto itr = params.begin(); itr != params.end();
         std::advance(itr, stride))
    {
        auto& param = *itr;
        SC_ASSERT(isa<ir::PointerType>(param.type()),
                  "First one or two arguments of constructor or destructor "
                  "must be pointers");
        auto* value =
            insert<ir::GetElementPointer>(before, inType, &param, index,
                                          std::array<size_t, 0>{}, "mem.acc");
        args.push_back(value);
        if (++loopIndex >= numParams) {
            break;
        }
    }
    return args;
}
