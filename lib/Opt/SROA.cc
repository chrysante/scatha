#include "Opt/Passes.h"

#include <iostream>
#include <memory>

#include <range/v3/algorithm.hpp>
#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "Opt/AccessTree.h"
#include "Opt/Common.h"
#include "Opt/PassRegistry.h"

using namespace scatha;
using namespace ir;
using namespace opt;

SC_REGISTER_PASS(opt::sroa, "sroa");

namespace {

using AccessTreeNode = opt::AccessTree<Alloca*>;

struct VariableContext {
    explicit VariableContext(Alloca* address, ir::Context& irCtx):
        irCtx(irCtx), baseAlloca(address) {}

    bool buildAccessTree();

    void slice();

    void cleanUnusedLoads();

    void cleanUnusedAddresses(Instruction* address);

    /// Build the access tree with using all the GEPs that directly or
    /// indirectly access this alloca.
    bool buildAccessTreeImpl(AccessTreeNode* parent,
                             GetElementPointer* address);

    bool buildAccessTreeVisitUsers(AccessTreeNode* parent,
                                   Instruction* address);

    void addAllocasForLeaves();

    void replaceBySlices();

    void replaceBySlicesImpl(Instruction* address, AccessTreeNode* node);

    void accessTreeLeafWalk(
        AccessTreeNode* root,
        utl::function_view<void(AccessTreeNode*, std::span<size_t const>)>
            callback);

    void accessTreePreorderWalk(
        utl::function_view<void(size_t level, AccessTreeNode*)> callback);

    void accessTreePostorderWalk(
        utl::function_view<void(size_t level, AccessTreeNode*)> callback);

    void printAccessTree() {
        std::cout << "%" << baseAlloca->name() << ":" << std::endl;
        for (auto& node: accessForest) {
            printAccessTreeImpl(node.get(), 0);
        }
    }

    void printAccessTreeImpl(AccessTreeNode const* node, int level) {
        for (int i = 0; i < level; ++i) {
            std::cout << "    ";
        }
        if (node->index()) {
            std::cout << *node->index() << " ";
        }
        std::cout << "[" << node->type()->name() << "]";
        std::cout << std::endl;
        for (auto* c: node->children()) {
            printAccessTreeImpl(c, level + 1);
        }
    }

    ir::Context& irCtx;

    /// The address of the structure we are trying to slice.
    Alloca* baseAlloca;

    /// Maps `getelementptr` instructions to their corresponding nodes in the
    /// access tree.
    utl::hashmap<Instruction*, AccessTreeNode*> addressToTreeNodes;

    /// While building the access tree we collect all loads to later erase all
    /// unused loads
    utl::small_vector<Load*, 32> loads;

    /// The access trees. This has as many elements as there are array accesses
    /// by GEP instructions.
    utl::small_vector<std::unique_ptr<AccessTreeNode>> accessForest;
};

} // namespace

bool opt::sroa(ir::Context& irCtx, ir::Function& function) {
    utl::small_vector<Alloca*> allocas;
    for (auto& inst: function.entry()) {
        if (auto* allocaInst = dyncast<Alloca*>(&inst)) {
            allocas.push_back(allocaInst);
        }
    }
    bool modifiedAny = false;
    for (auto* address: allocas) {
        VariableContext varInfo(address, irCtx);
        if (!varInfo.buildAccessTree()) {
            continue;
        }
        varInfo.slice();
        varInfo.cleanUnusedAddresses(address);
        modifiedAny = true;
    }
    if (modifiedAny) {
        ir::assertInvariants(irCtx, function);
    }
    return modifiedAny;
}

void VariableContext::slice() {
    addAllocasForLeaves();
    cleanUnusedLoads();
    replaceBySlices();
}

bool VariableContext::buildAccessTree() {
    SC_ASSERT(accessForest.empty(), "");
    accessForest.push_back(
        std::make_unique<AccessTreeNode>(baseAlloca->allocatedType()));
    auto* root = accessForest.front().get();
    addressToTreeNodes.insert({ baseAlloca, root });
    if (!buildAccessTreeVisitUsers(nullptr, baseAlloca)) {
        return false;
    }
    return true;
}

/// First this gets called with the `alloca` instruction and then recursively
/// with all users that are `gep`'s
bool VariableContext::buildAccessTreeImpl(AccessTreeNode* node,
                                          GetElementPointer* gep) {
    SC_ASSERT(!addressToTreeNodes.contains(gep), "gep graph should be acyclic");
    if (!gep->hasConstantArrayIndex()) {
        return false;
    }
    /// If node pointer is null then we may have non zero array index.
    /// We check for that also in the loop to access the correct type.
    if (!node) {
        SC_ASSERT(gep->basePointer() == this->baseAlloca,
                  "We are level 1 so we may have non-zero array index");
        size_t const arrayIndex = gep->constantArrayIndex();
        if (accessForest.size() <= arrayIndex) {
            accessForest.resize(arrayIndex + 1);
        }
        if (!accessForest[arrayIndex]) {
            accessForest[arrayIndex] =
                std::make_unique<AccessTreeNode>(baseAlloca->allocatedType());
        }
        node = accessForest[arrayIndex].get();
    }
    /// Else we may not have non-zero constant array index
    else if (gep->constantArrayIndex() != 0) {
        return false;
    }
    for (size_t index: gep->memberIndices()) {
        node->fanOut();
        node = node->childAt(index);
    }
    addressToTreeNodes.insert({ gep, node });
    return buildAccessTreeVisitUsers(node, gep);
}

bool VariableContext::buildAccessTreeVisitUsers(AccessTreeNode* node,
                                                Instruction* address) {
    bool haveGEPs = false;
    for (auto* user: address->users()) {
        // clang-format off
        bool flag = visit(*user, utl::overload{
            [](User const& user) {
                /// If any user is not a `load`, `store` or `getelementptr`
                /// instruction, we don't consider this `alloca`
                return false;
            },
            [&](Load& load) {
                loads.push_back(&load);
                return true;
            },
            [&](Store const& store) {
                /// For `store`s we also need to check if we are actually the
                /// address and not the value argument.
                return store.value() != address;
            },
            [&](GetElementPointer& gep) {
                haveGEPs = true;
                return buildAccessTreeImpl(node, &gep);
            }
            
        }); // clang-format on
        if (!flag) {
            return false;
        }
    }
    if (address == baseAlloca) {
        return haveGEPs;
    }
    return true;
}

void VariableContext::cleanUnusedLoads() {
    for (auto& inst: loads) {
        if (inst->users().empty()) {
            inst->parent()->erase(inst);
            inst = nullptr;
        }
    }
}

void VariableContext::cleanUnusedAddresses(Instruction* address) {
    auto users = address->users() | ToSmallVector<>;
    for (auto* user: users) {
        if (auto* gep = dyncast<GetElementPointer*>(user)) {
            cleanUnusedAddresses(gep);
        }
    }
    if (address->users().empty()) {
        address->parent()->erase(address);
    }
}

static std::string makeName(Value const* original,
                            std::span<std::size_t const> indices) {
    std::string result = std::string(original->name());
    for (size_t index: indices) {
        result += utl::strcat("_", index);
    }
    return result;
}

void VariableContext::addAllocasForLeaves() {
    for (auto& root: accessForest) {
        if (!root) {
            continue;
        }
        accessTreeLeafWalk(root.get(),
                           [&](AccessTreeNode* node,
                               std::span<size_t const> indices) {
            SC_ASSERT(node->type(), "We need to know what we allocate");
            auto* newScalar =
                new Alloca(irCtx, node->type(), makeName(baseAlloca, indices));
            node->setPayload(newScalar);
            baseAlloca->parent()->insert(baseAlloca, newScalar);
        });
    }
}

void VariableContext::replaceBySlices() {
    for (auto [addr, node]: addressToTreeNodes) {
        replaceBySlicesImpl(addr, node);
    }
}

void VariableContext::replaceBySlicesImpl(Instruction* address,
                                          AccessTreeNode* node) {
    /// Make a copy of the user list because we edit the user list during
    /// traversal.
    auto users = address->users() | ToSmallVector<>;
    for (auto* user: users) {
        auto* bb = user->parent();
        // clang-format off
        visit(*user, utl::overload{
            [&](Load& load) {
                if (!node->hasChildren()) {
                    load.setAddress(node->payload());
                    return;
                }
                Value* aggregate = irCtx.undef(load.type());
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf,
                                             std::span<size_t const> indices) {
                    auto* newLoad =
                        new Load(leaf->payload(),
                                 leaf->payload()->allocatedType(),
                                 std::string(load.name()));
                    bb->insert(&load, newLoad);
                    aggregate = new InsertValue(aggregate,
                                                newLoad,
                                                indices,
                                                std::string(load.name()));
                    bb->insert(&load, cast<Instruction*>(aggregate));
                });
                load.replaceAllUsesWith(aggregate);
                bb->erase(&load);
            },
            [&](Store& store) {
                if (!node->hasChildren()) {
                    store.setAddress(node->payload());
                    return;
                }
                auto* storedValue = store.value();
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf,
                                             std::span<size_t const> indices) {
                    auto* slice =
                        new ExtractValue(storedValue,
                                         indices,
                                         std::string(leaf->payload()->name()));
                    store.parent()->insert(&store, slice);
                    auto* newStore = new Store(irCtx, leaf->payload(), slice);
                    bb->insert(&store, newStore);
                });
                bb->erase(&store);
            },
            [&](GetElementPointer& gep) {},
            [&](Instruction& inst) { SC_UNREACHABLE(); },
        }); // clang-format on
    }
}

void VariableContext::accessTreeLeafWalk(
    AccessTreeNode* root,
    utl::function_view<void(AccessTreeNode*, std::span<size_t const>)>
        callback) {
    SC_ASSERT(root, "");
    utl::small_vector<size_t> indices;
    auto impl = [&](auto* node, auto impl) -> void {
        if (node->hasChildren()) {
            indices.push_back(0);
            for (auto* child: node->children()) {
                impl(child, impl);
                ++indices.back();
            }
            indices.pop_back();
        }
        else {
            callback(node, indices);
        }
    };
    impl(root, impl);
}

void VariableContext::accessTreePreorderWalk(
    utl::function_view<void(size_t level, AccessTreeNode*)> callback) {
    size_t l = 0;
    auto impl = [&](auto* node, auto impl) -> void {
        if (!node) {
            return;
        }
        callback(l, node);
        ++l;
        for (auto* child: node->children()) {
            impl(child, impl);
        }
        --l;
    };
    for (auto& root: accessForest) {
        l = 0;
        impl(root.get(), impl);
    }
}

void VariableContext::accessTreePostorderWalk(
    utl::function_view<void(size_t level, AccessTreeNode*)> callback) {
    size_t l = 0;
    auto impl = [&](auto* node, auto impl) -> void {
        if (!node) {
            return;
        }
        ++l;
        for (auto* child: node->children()) {
            impl(child, impl);
        }
        --l;
        callback(l, node);
    };
    for (auto& root: accessForest) {
        l = 0;
        impl(root.get(), impl);
    }
}
