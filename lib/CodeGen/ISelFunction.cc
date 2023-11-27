#include "CodeGen/ISelFunction.h"

#include <range/v3/view.hpp>
#include <utl/functional.hpp>
#include <utl/hashtable.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "CodeGen/SelectionDAG.h"
#include "IR/CFG.h"
#include "IR/Type.h"
#include "MIR/CFG.h"

using namespace scatha;
using namespace cg;

static constexpr size_t WordSize = 8;

/// \Returns the number of machine words required to store values of type \p
/// type
static size_t numWords(ir::Type const* type) {
    return utl::ceil_divide(type->size(), WordSize);
}

/// \Returns the number of machine words required to store the value \p value
static size_t numWords(ir::Value const* value) {
    return numWords(value->type());
}

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

struct ISelContext {
    ir::Function const& irFn;
    mir::Function const& mirFn;
    ValueMap const& valueMap;

    AllocaMap allocaMap;

    ISelContext(ir::Function const& irFn,
                mir::Function const& mirFn,
                ValueMap const& valueMap):
        irFn(irFn), mirFn(mirFn), valueMap(valueMap) {}

    void run();

    void computeAllocaMap();
};

} // namespace

void cg::iselFunction(ir::Function const& irFn,
                      mir::Function const& mirFn,
                      ValueMap const& valueMap) {
    ISelContext context(irFn, mirFn, valueMap);
    context.run();
}

void ISelContext::run() {
    computeAllocaMap();
    for (auto& BB: irFn) {
        auto DAG = SelectionDAG::Build(BB);
        generateGraphvizTmp(DAG);
    }
}

static size_t alignTo(size_t size, size_t align) {
    if (size % align == 0) {
        return size;
    }
    return size + align - size % align;
}

void ISelContext::computeAllocaMap() {
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
