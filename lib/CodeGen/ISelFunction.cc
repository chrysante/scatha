#include "CodeGen/ISelFunction.h"

#include <queue>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CodeGen/ISelCommon.h"
#include "CodeGen/SelectionDAG.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "MIR/CFG.h"
#include "MIR/Instructions.h"
#include "MIR/Module.h"

using namespace scatha;
using namespace cg;

static void copyDependencies(SelectionNode& oldDepender,
                             SelectionNode& newDepender) {
    auto impl = [&](auto get, auto add) {
        for (auto* dependency: std::invoke(get, oldDepender) | ToSmallVector<>)
        {
            std::invoke(add, newDepender, dependency);
        }
    };
    impl([](auto& node) { return node.valueDependencies(); },
         &SelectionNode::addValueDependency);
    impl([](auto& node) { return node.executionDependencies(); },
         &SelectionNode::addExecutionDependency);
}

/// Used to calculate width of a slice when issuing multiple copy
/// instructions for large types.
static size_t sliceWidth(size_t numBytes, size_t index, size_t numWords) {
    if (index != numWords - 1) {
        return 8;
    }
    size_t res = numBytes % 8;
    return res == 0 ? 8 : res;
}

namespace {

template <typename Inst>
using Matcher = std::function<bool(Inst const& inst, SelectionNode& node)>;

/// Since we only issue one instruction for all static allocas we need to store
/// the offsets that represent the individual IR allocas
struct AllocaLocation {
    /// Pointer to the base of the current functions stack frame
    mir::Register* baseptr;

    /// Offset from the base of this alloca region
    size_t offset;
};

///
class AllocaMap {
public:
    AllocaLocation operator()(ir::Alloca const* key) const {
        auto itr = map.find(key);
        SC_EXPECT(itr != map.end());
        return itr->second;
    }

    void insert(ir::Alloca const* key, AllocaLocation loc) {
        map.insert({ key, loc });
    }

private:
    utl::hashmap<ir::Alloca const*, AllocaLocation> map;
};

struct FunctionContext {
    ///

    ir::Function const& irFn;
    mir::Module& mirMod;
    mir::Function& mirFn;
    ValueMap& globalMap;

    ///
    ValueMap localMap;
    AllocaMap allocaMap;

    FunctionContext(ir::Function const& irFn,
                    mir::Module& mirMod,
                    mir::Function& mirFn,
                    ValueMap& globalMap):
        irFn(irFn), mirMod(mirMod), mirFn(mirFn), globalMap(globalMap) {}

    /// Run the lowering algorithm for `irFn`
    void run();

    /// Allocates a `mir::BasicBlock` and inserts it into `mirFn`
    mir::BasicBlock* declareBB(ir::BasicBlock const& irBB);

    ///
    void computeAllocaMap(mir::BasicBlock& mirEntry);
};

struct BBContext {
    ir::BasicBlock const& irBB;
    mir::Module& mirMod;
    mir::Function& mirFn;
    mir::BasicBlock& mirBB;
    ValueMap& globalMap;
    ValueMap& localMap;
    SelectionDAG DAG;

    /// List of instructions that will be populated for every node and then
    /// moved to that node
    List<mir::Instruction> instructions;

    BBContext(ir::BasicBlock const& irBB,
              mir::Module& mirMod,
              mir::Function& mirFn,
              mir::BasicBlock& mirBB,
              ValueMap& globalMap,
              ValueMap& localMap):
        irBB(irBB),
        mirMod(mirMod),
        mirFn(mirFn),
        mirBB(mirBB),
        globalMap(globalMap),
        localMap(localMap),
        DAG(SelectionDAG::Build(irBB)) {}

    void run();

    void match(SelectionNode& node);

    /// Emits the instruction \p inst to the current node. Takes ownership of
    /// the pointer
    void emit(mir::Instruction* inst) { instructions.push_back(inst); }

    /// Maps IR values to MIR values. In particular:
    /// ```
    /// Functions    -> Functions
    /// Basic Blocks -> Basic Blocks
    /// Instructions -> Registers
    /// Constants    -> Constants
    /// ```
    /// Return type can be explicitly specified. Will trap if specified return
    /// type does not match
    template <typename V = mir::Value>
    V* resolve(ir::Value const& value) {
        return cast<V*>(resolveImpl(value));
    }

    mir::Register* resolve(ir::Instruction const& inst) {
        return resolve<mir::Register>(inst);
    }

    mir::Register* resolve(ir::Parameter const& param) {
        return resolve<mir::Register>(param);
    }

    mir::Function* resolve(ir::Function const& func) {
        return resolve<mir::Function>(func);
    }

    mir::BasicBlock* resolve(ir::BasicBlock const& bb) {
        return resolve<mir::BasicBlock>(bb);
    }

    /// Resolves the value and generates a copy instruction if necessary
    mir::SSARegister* resolveToRegister(Metadata metadata,
                                        ir::Value const& value);

    /// \Returns The register after \p dest
    template <typename R>
    R* genCopy(R* dest, mir::Value* source, size_t numBytes, Metadata metadata);

    template <typename R>
    R* genCondCopy(R* dest,
                   mir::Value* source,
                   size_t numBytes,
                   mir::CompareOperation condition,
                   Metadata metadata);

    template <typename R>
    R* genCopyImpl(R* dest,
                   mir::Value* source,
                   size_t numBytes,
                   Metadata metadata,
                   auto insertCallback);

    mir::SSARegister* nextRegister(size_t numWords = 1) {
        return nextRegistersFor(numWords, nullptr);
    }

    mir::SSARegister* nextRegistersFor(ir::Value const* value) {
        return nextRegistersFor(numWords(value), value);
    }

    mir::SSARegister* nextRegistersFor(size_t numWords,
                                       ir::Value const* liveWith);

    mir::MemoryAddress computeGep(ir::GetElementPointer const* gep);

private:
    /// Stub that we'll delete later
    void matchImpl(ir::Value const&, SelectionNode&) {
        //        SC_UNREACHABLE();
    }

    void matchImpl(ir::Call const& inst, SelectionNode& node);
    void matchImpl(ir::Load const& inst, SelectionNode& node);
    void matchImpl(ir::ArithmeticInst const& inst, SelectionNode& node);

    template <typename Inst>
    void applyMatchers(std::span<Matcher<Inst> const> matchers,
                       Inst const& inst,
                       SelectionNode& node);

    /// Sets the computed value and selected instruction to the node
    void finalizeNode(SelectionNode& node);

    mir::Value* resolveImpl(ir::Value const&);
};

} // namespace

void cg::iselFunction(ir::Function const& irFn,
                      mir::Module& mirMod,
                      mir::Function& mirFn,
                      ValueMap& globalMap) {
    FunctionContext context(irFn, mirMod, mirFn, globalMap);
    context.run();
}

void FunctionContext::run() {
    /// Declare all basic blocks
    for (auto& irBB: irFn) {
        declareBB(irBB);
    }
    computeAllocaMap(*mirFn.entry());
    /// Associate parameters with bottom registers.
    auto regItr = mirFn.ssaRegisters().begin();
    for (auto& param: irFn.parameters()) {
        localMap.insert(&param, regItr.to_address());
        std::advance(regItr, numWords(&param));
    }
    /// Generate code for all blocks
    for (auto& irBB: irFn) {
        BBContext BBCtx(irBB,
                        mirMod,
                        mirFn,
                        *localMap(&irBB),
                        globalMap,
                        localMap);
        BBCtx.run();
    }
}

mir::BasicBlock* FunctionContext::declareBB(ir::BasicBlock const& irBB) {
    auto* mirBB = new mir::BasicBlock(&irBB);
    mirFn.pushBack(mirBB);
    localMap.insert(&irBB, mirBB);
    return mirBB;
}

static size_t alignTo(size_t size, size_t align) {
    if (size % align == 0) {
        return size;
    }
    return size + align - size % align;
}

void FunctionContext::computeAllocaMap(mir::BasicBlock& mirEntry) {
    auto& entry = irFn.entry();
    auto allocas = entry | ranges::views::take_while(isa<ir::Alloca>) |
                   ranges::views::transform(cast<ir::Alloca const&>);
    size_t offset = 0;
    utl::small_vector<size_t> offsets;
    /// Compute the offsets for the allocas
    SC_ASSERT(ranges::all_of(allocas, &ir::Alloca::isStatic),
              "For now we only support lowering static allocas");
    for (auto& inst: allocas) {
        offsets.push_back(offset);
        auto size = inst.allocatedSize();
        size_t const StaticAllocaAlign = 16;
        offset += alignTo(*size, StaticAllocaAlign);
    }

    /// Emit one `lincsp` instruction
    mir::Register* baseptr = nullptr; // TODO: Implement this

    /// Store the results in the map
    for (auto [inst, offset]: ranges::views::zip(allocas, offsets)) {
        allocaMap.insert(&inst, { baseptr, offset });
    }
}

static utl::small_vector<SelectionNode*> topsort(SelectionNode* term) {
    utl::small_vector<SelectionNode*> result;
    utl::hashset<SelectionNode const*> marked;
    auto visit = [&](auto visit, SelectionNode* node) {
        if (marked.contains(node)) {
            return;
        }
        auto dependencies = ranges::views::concat(node->executionDependencies(),
                                                  node->valueDependencies() |
                                                      ranges::views::filter(
                                                          [&](auto* node) {
            auto* inst = dyncast<ir::Instruction const*>(node->irValue());
            return inst &&
                   inst->parent() ==
                       cast<ir::Instruction const*>(term->irValue())->parent();
                                                      }));
        for (auto* dep: dependencies) {
            visit(visit, dep);
        }
        marked.insert(node);
        result.push_back(node);
    };
    visit(visit, term);
    ranges::reverse(result);
    return result;
}

void BBContext::run() {
    generateGraphvizTmp(DAG, utl::strcat(irBB.name(), ".before"));
    for (auto* node: topsort(DAG[irBB.terminator()])) {
        match(*node);
    }
    generateGraphvizTmp(DAG, utl::strcat(irBB.name(), ".after"));
}

void BBContext::match(SelectionNode& node) {
    visit(*node.irValue(), [&](auto& value) { matchImpl(value, node); });
}

void BBContext::matchImpl(ir::Call const& call, SelectionNode& node) {
    utl::small_vector<mir::Value*, 16> args;
    for (auto* arg: call.arguments()) {
        auto* mirArg = resolve(*arg);
        size_t const numWords = ::numWords(arg);
        for (size_t i = 0; i < numWords; ++i, mirArg = mirArg->next()) {
            args.push_back(mirArg);
        }
    }
    size_t const numDests = numWords(call.type());
    auto* dest = resolve(call);
    // clang-format off
    SC_MATCH (*call.function()) {
        [&](ir::Value const& func) {
            emit(new mir::CallInst(dest,
                                   numDests,
                                   resolve(func),
                                   std::move(args),
                                   call.metadata()));
        },
        [&](ir::ForeignFunction const& func) {
            emit(new mir::CallExtInst(dest,
                                      numDests,
                                      { .slot  = static_cast<uint32_t>(func.slot()),
                                        .index = static_cast<uint32_t>(func.index()) },
                                      std::move(args),
                                      call.metadata()));
        },
    }; // clang-format on
    finalizeNode(node);
}

void BBContext::matchImpl(ir::Load const& load, SelectionNode& node) {}

void BBContext::matchImpl(ir::ArithmeticInst const& inst, SelectionNode& node) {
    utl::small_vector<Matcher<ir::ArithmeticInst>> matchers;
    // Arithmetic -> Load  [ -> GEP ]
    matchers.push_back(
        [this](ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* load = dyncast<ir::Load const*>(inst.rhs());
        if (!load) {
            return false;
        }
        auto& loadNode = *DAG[load];
        copyDependencies(loadNode, node);
        node.removeDependency(&loadNode);
        auto src = [&] {
            auto* gep = dyncast<ir::GetElementPointer const*>(load->address());
            if (gep) {
                auto& gepNode = *DAG[gep];
                copyDependencies(gepNode, node);
                node.removeDependency(&gepNode);
                return computeGep(gep);
            }
            return mir::MemoryAddress(
                resolveToRegister(load->metadata(), *load->address()));
        }();
        auto* lhs = resolveToRegister(inst.metadata(), *inst.lhs());
        size_t size = inst.lhs()->type()->size();
        emit(new mir::LoadArithmeticInst(resolve(inst),
                                         lhs,
                                         src,
                                         size,
                                         inst.operation(),
                                         inst.metadata()));
        return true;
    });
    // Arithmetic (base case)
    matchers.push_back(
        [this](ir::ArithmeticInst const& inst, SelectionNode& node) {
        auto* lhs = resolveToRegister(inst.metadata(), *inst.lhs());
        auto* rhs = resolve(*inst.rhs());
        size_t size = inst.lhs()->type()->size();
        emit(new mir::ValueArithmeticInst(resolve(inst),
                                          lhs,
                                          rhs,
                                          size,
                                          inst.operation(),
                                          inst.metadata()));
        return true;
    });

    applyMatchers<ir::ArithmeticInst>(matchers, inst, node);
}

template <typename Inst>
void BBContext::applyMatchers(std::span<Matcher<Inst> const> matchers,
                              Inst const& inst,
                              SelectionNode& node) {
    for (auto& matcher: matchers) {
        if (matcher(inst, node)) {
            break;
        }
    }
    finalizeNode(node);
}

void BBContext::finalizeNode(SelectionNode& node) {
    /// Must be an instruction because we can only finalize instruction nodes
    auto& inst = cast<ir::Instruction const&>(*node.irValue());
    node.setMIR(resolve(inst), std::move(instructions));
}

mir::Value* BBContext::resolveImpl(ir::Value const& irVal) {
    if (auto* result = globalMap(&irVal)) {
        return result;
    }
    if (auto* result = localMap(&irVal)) {
        return result;
    }
    // clang-format off
    return visit(irVal, utl::overload{
        [&](ir::Instruction const& inst) -> mir::Register* {
            if (isa<ir::VoidType>(inst.type())) {
                return nullptr;
            }
            auto* reg = nextRegistersFor(&inst);
            localMap.insert(&inst, reg);
            return reg;
        },
        [&](ir::GlobalVariable const& var) -> mir::Value* {
            SC_UNIMPLEMENTED();
        },
        [&](ir::IntegralConstant const& constant) {
            SC_ASSERT(constant.type()->bitwidth() <= 64, "Can't handle extended width integers");
            uint64_t value = constant.value().to<uint64_t>();
            auto* mirConst = mirMod.constant(value, constant.type()->size());
            globalMap.insert(&constant, mirConst);
            return mirConst;
        },
        [&](ir::FloatingPointConstant const& constant) -> mir::Value* {
            SC_ASSERT(constant.type()->bitwidth() <= 64, "Can't handle extended width floats");
            uint64_t value = 0;
            if (constant.value().precision() == APFloatPrec::Single) {
                value = utl::bit_cast<uint32_t>(constant.value().to<float>());
            }
            else {
                value = utl::bit_cast<uint64_t>(constant.value().to<double>());
            }
            auto* mirConst = mirMod.constant(value, constant.type()->size());
            globalMap.insert(&constant, mirConst);
            return mirConst;
        },
        [&](ir::NullPointerConstant const& constant) -> mir::Value* {
            auto* mirConstant = mirMod.constant(0, 8);
            globalMap.insert(&constant, mirConstant);
            return mirConstant;
        },
        [&](ir::RecordConstant const& value) -> mir::Value* {
            SC_UNIMPLEMENTED();
        },
        [&](ir::UndefValue const&) -> mir::Value* {
            return mirMod.undefValue();
        },
        [](ir::Value const& value) -> mir::Value* {
            SC_UNREACHABLE("Everything else shall be forward declared");
        }
    }); // clang-format on
}

template <typename R>
R* BBContext::genCopy(R* dest,
                      mir::Value* source,
                      size_t numBytes,
                      Metadata metadata) {
    return genCopyImpl(dest,
                       source,
                       numBytes,
                       metadata,
                       [&](auto* dest, auto* source, size_t numBytes) {
        emit(new mir::CopyInst(dest, source, numBytes, metadata));
    });
}

template <typename R>
R* BBContext::genCondCopy(R* dest,
                          mir::Value* source,
                          size_t numBytes,
                          mir::CompareOperation condition,
                          Metadata metadata) {
    return genCopyImpl(dest,
                       source,
                       numBytes,
                       metadata,
                       [&](auto* dest, auto* source, size_t numBytes) {
        emit(
            new mir::CondCopyInst(dest, source, numBytes, condition, metadata));
    });
}

template <typename R>
R* BBContext::genCopyImpl(R* dest,
                          mir::Value* source,
                          size_t numBytes,
                          Metadata metadata,
                          auto insertCallback) {
    size_t const numWords = utl::ceil_divide(numBytes, 8);
    for (size_t i = 0; i < numWords;
         ++i, dest = dest->next(), source = source->next())
    {
        insertCallback(dest, source, sliceWidth(numBytes, i, numWords));
    }
    return dest;
}

mir::SSARegister* BBContext::nextRegistersFor(size_t numWords,
                                              ir::Value const* value) {
    auto* result = new mir::SSARegister();
    mirFn.ssaRegisters().add(result);
    for (size_t i = 1; i < numWords; ++i) {
        auto* r = new mir::SSARegister();
        mirFn.ssaRegisters().add(r);
    }
    return result;
}

mir::SSARegister* BBContext::resolveToRegister(Metadata metadata,
                                               ir::Value const& value) {
    auto* result = resolve(value);
    if (auto* reg = dyncast<mir::SSARegister*>(result)) {
        return reg;
    }
    SC_UNIMPLEMENTED();
}

mir::MemoryAddress BBContext::computeGep(ir::GetElementPointer const* gep) {
    auto* base = resolve(*gep->basePointer());
    if (auto* undef = dyncast<mir::UndefValue*>(base)) {
        return mir::MemoryAddress(nullptr, nullptr, 0, 0);
    }
    if (auto* constant = dyncast<mir::Constant*>(base)) {
        SC_UNIMPLEMENTED();
    }
    mir::Register* basereg = cast<mir::Register*>(base);
    auto [dynFactor, constFactor, constOffset] =
        [&]() -> std::tuple<mir::Register*, size_t, size_t> {
        size_t elemSize = gep->inboundsType()->size();
        size_t innerOffset = gep->innerByteOffset();
        auto* constIndex =
            dyncast<ir::IntegralConstant const*>(gep->arrayIndex());
        if (constIndex && constIndex->value() == 0) {
            return { nullptr, elemSize, innerOffset };
        }
        mir::Value* arrayIndex = resolve(*gep->arrayIndex());
        if (auto* regArrayIdx = dyncast<mir::Register*>(arrayIndex)) {
            return { regArrayIdx, elemSize, innerOffset };
        }
        if (auto* constArrIdx = dyncast<mir::Constant*>(arrayIndex)) {
            size_t totalOffset = constArrIdx->value() * elemSize + innerOffset;
            // FIXME: This should not be hard coded
            if (totalOffset < 256) {
                return { nullptr, 0, totalOffset };
            }
        }
        auto* result = nextRegistersFor(1, gep);
        genCopy(result, arrayIndex, 8, gep->metadata());
        return { result, elemSize, innerOffset };
    }();
    return mir::MemoryAddress(basereg,
                              dynFactor,
                              utl::narrow_cast<uint32_t>(constFactor),
                              utl::narrow_cast<uint32_t>(constOffset));
}
