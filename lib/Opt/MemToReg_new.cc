#include "MemToReg_new.h"

#include <utl/hashtable.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Validate.h"
#include "Opt/Dominance.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

struct VariableInfo {
    void setVersion(size_t index, Value* value) {
        if (versions.size() < index + 1) {
            versions.resize(index + 1);
        }
        versions[index] = value;
    }

    Type const* type = nullptr;
    utl::small_vector<Load*> loads;
    utl::small_vector<Store*> stores;
    utl::hashmap<BasicBlock*, Phi*> phiNodes;
    utl::hashset<BasicBlock*> blocksWithStores;
    utl::stack<size_t> stack;
    utl::small_vector<Value*> versions;
    size_t counter = 0;
};

struct MemToRegContext {
    MemToRegContext(Context& irCtx, Function& function):
        irCtx(irCtx),
        function(function),
        domSets(computeDominanceSets(function)),
        domTree(buildDomTree(function, domSets)),
        domFronts(computeDominanceFrontiers(function, domTree)) {}

    bool run();

    VariableInfo gatherInfo(Alloca* address);

    void insertPhis(Alloca* address, VariableInfo& info);

    void renameVariables(BasicBlock* basicBlock);

    void genName(Alloca* addr, Value* value);

    bool clean();

    Context& irCtx;
    Function& function;
    DominanceMap domSets;
    DomTree domTree;
    DominanceFrontierMap domFronts;
    utl::hashmap<Alloca*, VariableInfo> variables;
    utl::hashset<BasicBlock const*> renamedBlocks;
    /// Map phis to corresponding allocas
    utl::hashmap<Phi*, Alloca*> phiMap;
};

} // namespace

static bool isPromotable(Alloca const& allc) {
    for (auto* user: allc.users()) {
        if (!isa<Load>(user) && !isa<Store>(user)) {
            return false;
        }
    }
    return true;
}

bool opt::memToReg(Context& irCtx, Function& function) {
    MemToRegContext ctx(irCtx, function);
    bool const result = ctx.run();
    assertInvariants(irCtx, function);
    return result;
}

bool MemToRegContext::run() {
    variables =
        function.entry() | ranges::views::filter([](Instruction& inst) {
            return isa<Alloca>(inst) && isPromotable(cast<Alloca const&>(inst));
        }) |
        ranges::views::transform([this](Instruction& inst) {
            auto* addr = dyncast<Alloca*>(&inst);
            return std::pair{ addr, gatherInfo(addr) };
        }) |
        ranges::to<utl::hashmap<Alloca*, VariableInfo>>;
    for (auto&& [addr, info]: variables) {
        insertPhis(addr, info);
    }
    renameVariables(&function.entry());
    return clean();
}

VariableInfo MemToRegContext::gatherInfo(Alloca* address) {
    VariableInfo result;
    result.type = address->allocatedType();
    for (auto* user: address->users()) {
        visit(*user,
              utl::overload{
                  [&](Store& store) {
            result.stores.push_back(&store);
            result.blocksWithStores.insert(store.parent());
                  },
                  [&](Load& load) { result.loads.push_back(&load); },
                  [&](Instruction const& inst) { SC_UNREACHABLE(); } });
    }
    return result;
}

void MemToRegContext::insertPhis(Alloca* address, VariableInfo& varInfo) {
    SC_ASSERT(isPromotable(*address), "");
    auto worklist =
        varInfo.blocksWithStores | ranges::to<utl::small_vector<BasicBlock*>>;
    auto everOnWorklist = worklist;
    while (!worklist.empty()) {
        BasicBlock* x = worklist.back();
        worklist.pop_back();
        auto& dfX = domFronts[x];
        for (auto* y: dfX) {
            if (varInfo.phiNodes.contains(y)) {

                continue;
            }
            auto* undefVal = irCtx.undef(varInfo.type);
            auto phiArgs   = y->predecessors() |
                           ranges::views::transform([&](BasicBlock* pred) {
                               return PhiMapping(pred, undefVal);
                           }) |
                           ranges::to<utl::small_vector<PhiMapping>>;
            auto* phi =
                new Phi(std::move(phiArgs),
                        irCtx.uniqueName(&function,
                                         utl::strcat(address->name(), ".phi")));
            phiMap[phi] = address;
            y->pushFront(phi);
            varInfo.phiNodes[y] = phi;
            worklist.push_back(y);
            everOnWorklist.push_back(y);
        }
    }
}

void MemToRegContext::genName(Alloca* addr, Value* value) {
    auto& info     = variables[addr];
    size_t const i = info.counter;
    info.setVersion(i, value);
    info.stack.push(i);
    info.counter = i + 1;
}

void MemToRegContext::renameVariables(BasicBlock* basicBlock) {
    if (renamedBlocks.contains(basicBlock)) {
        return;
    }
    renamedBlocks.insert(basicBlock);
    for (auto& phi: basicBlock->phiNodes()) {
        genName(phiMap[&phi], &phi);
    }
    for (auto& inst: *basicBlock) {
        for (auto [index, operand]: inst.operands() | ranges::views::enumerate)
        {
            auto* load = dyncast<Load*>(operand);
            if (!load) {
                continue;
            }
            if (auto* allc = static_cast<Alloca*>(load->address());
                !variables.contains(allc))
            {
                continue;
            }
            auto* address = cast<Alloca*>(load->address());
            auto& info    = variables[address];
            size_t i      = info.stack.top();
            inst.setOperand(index, info.versions[i]);
        }
        auto* store = dyncast<Store*>(&inst);
        if (!store) {
            continue;
        }
        auto* address = static_cast<Alloca*>(store->dest());
        if (!variables.contains(address)) {
            continue;
        }
        genName(cast<Alloca*>(store->dest()), store->source());
    }
    for (auto succ: basicBlock->successors()) {
        for (auto& phi: succ->phiNodes()) {
            auto* address = phiMap[&phi];
            auto& info    = variables[address];
            if (info.stack.empty()) {
                continue;
            }
            size_t const i = info.stack.top();
            phi.setArgument(basicBlock, info.versions[i]);
        }
    }
    for (auto* succ: basicBlock->successors()) {
        renameVariables(succ);
    }
    for (auto& inst: *basicBlock) {
        // clang-format off
        auto* address = visit(inst, utl::overload{
            [&](Phi& phi) {
                return phiMap[&phi];
            },
            [&](Store& store) -> Alloca* {
                auto* addr = dyncast<Alloca*>(store.dest());
                if (!addr) {
                    return nullptr;
                }
                if (!variables.contains(addr)) {
                    return nullptr;
                }
                return addr;
            },
            [&](Instruction const& inst) { return nullptr; }
        }); // clang-format on
        if (!address) {
            continue;
        }
        auto& info = variables[address];
        info.stack.pop();
    }
}

bool MemToRegContext::clean() {
    bool cleanedAny = false;
    for (auto& [address, info]: variables) {
        for (auto* load: info.loads) {
            SC_ASSERT(load->users().empty(), "Should be empty after promotion");
            load->parent()->erase(load);
        }
        for (auto* store: info.stores) {
            store->parent()->erase(store);
        }
        SC_ASSERT(address->users().empty(), "Should be empty after promotion");
        address->parent()->erase(address);
        cleanedAny = true;
    }
    for (auto [phi, address]: phiMap) {
        if (phi->users().empty()) {
            phi->parent()->erase(phi);
            cleanedAny = true;
        }
    }
    return cleanedAny;
}
