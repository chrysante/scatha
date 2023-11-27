#include "CodeGen/ISelFunction.h"

#include <range/v3/view.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CodeGen/SelectionDAG.h"
#include "CodeGen/ISelCommon.h"
#include "CodeGen/ValueMap.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

namespace {

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
    mir::Function& mirFn;
    ValueMap const& globalMap;
    
    ///
    ValueMap localMap;
    AllocaMap allocaMap;

    FunctionContext(ir::Function const& irFn,
                    mir::Function& mirFn,
                    ValueMap const& globalMap):
        irFn(irFn), mirFn(mirFn), globalMap(globalMap) {}

    /// Run the lowering algorithm for `irFn`
    void run();

    /// Allocates a `mir::BasicBlock` and inserts it into `mirFn`
    mir::BasicBlock* declareBB(ir::BasicBlock const& irBB);
    
    ///
    void computeAllocaMap(mir::BasicBlock& mirEntry);
};

struct BBContext {
    ir::BasicBlock const& irBB;
    mir::BasicBlock& mirBB;
    ValueMap const& globalMap;
    ValueMap& localMap;
    SelectionDAG DAG;
    
    BBContext(ir::BasicBlock const& irBB,
              mir::BasicBlock& mirBB,
              ValueMap const& globalMap,
              ValueMap& localMap):
        irBB(irBB),
        mirBB(mirBB),
        globalMap(globalMap),
        localMap(localMap),
        DAG(SelectionDAG::Build(irBB)) {}
    
    void run();
    
    void match(SelectionNode* node);
    
    /// Stub that we'll delete later
    void matchImpl(ir::Value const*, SelectionNode*) {
        SC_UNREACHABLE();
    }
    
    void matchImpl(ir::Call const* inst, SelectionNode* node);
};

} // namespace

void cg::iselFunction(ir::Function const& irFn,
                      mir::Function& mirFn,
                      ValueMap const& globalMap) {
    FunctionContext context(irFn, mirFn, globalMap);
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

void BBContext::run() {
    for (auto* node: DAG.sideEffectNodes() | ranges::views::reverse) {
        match(node);
    }
    for (auto* node: DAG.outputNodes()) {
        if (!node->matched()) {
            match(node);
        }
    }
}

void BBContext::match(SelectionNode* node) {
    visit(*node->value(), [&](auto& value) {
        matchImpl(&value, node);
    });
}

void BBContext::matchImpl(ir::Call const* inst, SelectionNode* node) {
    SC_UNIMPLEMENTED();
}
