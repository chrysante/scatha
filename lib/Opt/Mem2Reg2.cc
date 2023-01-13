#include "Opt/Mem2Reg2.h"

#include <algorithm>

#include "Basic/Basic.h"
#include "IR/Context.h"
#include "IR/CFG.h"
#include "IR/Module.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace scatha::opt;
using namespace scatha::ir;

namespace {

template <typename I = Instruction>
struct InstructionContext {
    I* instruction;
    size_t positionInBB;
};

static constexpr auto lsCmpLs = [](auto const& a, auto const& b) { return a.positionInBB < b.positionInBB; };

struct LoadPromotionContext {
    explicit LoadPromotionContext(ir::Context& context, Function& function): irCtx(context), function(function) {}
    
    void run();
    
    bool promote(InstructionContext<Load>);
  
    void gather();
    
    ir::Context& irCtx;
    Function& function;
    
    /// Maps pairs of basic blocks and addresses to lists of load and store instructions from and to that address in that basic block.
    utl::hashmap<std::pair<BasicBlock*, Value*>, utl::vector<InstructionContext<Instruction>>> loadsAndStores;
    /// List of all load instructions in the function.
    utl::vector<InstructionContext<Load>> loads;
    /// Maps evicted instructions to their respective replacement values.
    utl::hashmap<Instruction const*, Value*> replacementMap;
};

} // namespace

static void promoteLoads(ir::Context& context, ir::Module& mod) {
    for (auto& function: mod.functions()) {
        LoadPromotionContext ctx(context, function);
        ctx.run();
    }
}

void opt::mem2Reg2(ir::Context& context, ir::Module& mod) {
    promoteLoads(context, mod);
}

void LoadPromotionContext::run() {
    gather();
    for (auto& load: loads) {
        promote(load);
    }
}

void LoadPromotionContext::gather() {
    for (auto& bb: function.basicBlocks()) {
        for (auto&& [index, inst]: utl::enumerate(bb.instructions)) {
            visit(inst, utl::overload{ // clang-format off
                [&, index = index](Load& load) {
                    loads.push_back({ &load, index });
                    loadsAndStores[{ load.parent(), load.address() }].push_back({ &load, index });
                },
                [&, index = index](Store& store) {
                    loadsAndStores[{ store.parent(), store.dest() }].push_back({ &store, index });
                },
                [&](auto&) {},
            }); // clang-format on
        }
    }
    /// Assertion code
    for (auto&& [bb, ls]: loadsAndStores) {
        SC_ASSERT(std::is_sorted(ls.begin(), ls.end(), lsCmpLs), "Loads and stores in one basic block must be sorted");
    }
}

namespace {

struct SearchContext {
    Value* search(BasicBlock*, size_t depth, size_t bifurkations);
    
    ir::Context& irCtx;
    ir::Function& function;
    utl::hashmap<std::pair<BasicBlock*, Value*>, utl::vector<InstructionContext<Instruction>>>& loadsAndStores;
    Load& load;
    size_t loadPositionInBB;
};

} // namespace

bool LoadPromotionContext::promote(InstructionContext<Load> loadContext) {
    auto* const load = loadContext.instruction;
    SearchContext ctx{ irCtx, function, loadsAndStores, *load, loadContext.positionInBB };
    Value* newValue = ctx.search(load->parent(), 0, 0);
    if (!newValue) {
        return false;
    }
    decltype(replacementMap)::iterator itr;
    while ((itr = replacementMap.find(newValue)) != replacementMap.end()) {
        newValue = itr->second;
    }
    replacementMap[load] = newValue;
    load->setName("evicted-load");
    load->parent()->instructions.erase(load);
    replaceValue(load, newValue);
    return true;
}

Value* SearchContext::search(BasicBlock* basicBlock, size_t depth, size_t bifurkations) {
    auto& ls = loadsAndStores[{ basicBlock, load.address() }];
    auto ourLoad = [&]{
        return std::find_if(ls.begin(), ls.end(), [&](InstructionContext<> const& c) { return c.positionInBB == loadPositionInBB; });
    };
    auto const beginItr = basicBlock != load.parent() || depth == 0 ? ls.begin() : ourLoad() + 1;
    auto const endItr = depth > 0 ? ls.end() : ourLoad();
    auto const itr = std::max_element(beginItr, endItr, lsCmpLs);
    if (itr != endItr) {
        /// This basic block has a load or store that we use to promote
        return visit(*itr->instruction, utl::overload{
            [](Load& load) { return &load; },
            [](Store& store) { return store.source(); },
            [](auto&) -> Value* { SC_UNREACHABLE(); }
        });
    }
    if (depth > 0 && basicBlock == load.parent()) {
        /// We are back in our staring basic block and haven't found a load or store to promote. What do we do now?
        SC_DEBUGFAIL();
    }
    /// This basic block has no load or store that we use to promote
    /// We need to visit our predecessors
    switch (basicBlock->predecessors.size()) {
    case 0:
        return nullptr;
    case 1:
        return search(basicBlock->predecessors.front(), depth + 1, bifurkations);
    default:
        utl::small_vector<PhiMapping> phiArgs;
        phiArgs.reserve(basicBlock->predecessors.size());
        for (auto* pred: basicBlock->predecessors) {
            PhiMapping arg{ pred, search(pred, depth + 1, bifurkations + 1) };
            SC_ASSERT(arg.value, "This probably just means we can't promote or have to insert a load into pred, but we figure it out later.");
            phiArgs.push_back(arg);
        }
        std::string name = bifurkations == 0 ? std::string(load.name()) : irCtx.uniqueName(&function, load.name(), ".p", bifurkations);
        auto* phi = new Phi(std::move(phiArgs), std::move(name));
        basicBlock->instructions.push_front(phi);
        return phi;
    }
}
