#include "Opt/SROA.h"

#include <iostream>
#include <memory>

#include <range/v3/algorithm.hpp>
#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/Type.h"
#include "Opt/Common.h"

using namespace scatha;
using namespace ir;
using namespace opt;

namespace {

struct AccessTreeNode {
    AccessTreeNode(size_t index = 0): index(index) {}

    void setChildAt(size_t index, std::unique_ptr<AccessTreeNode> node) {
        if (index >= children.size()) {
            children.resize(index + 1);
        }
        SC_ASSERT(children[index] == nullptr, "");
        node->parent    = this;
        node->index     = index;
        children[index] = std::move(node);
    }

    AccessTreeNode* childAt(size_t index) const {
        if (index < children.size()) {
            return children[index].get();
        }
        return nullptr;
    }

    Type const* type       = nullptr;
    Alloca* newScalarVar   = nullptr;
    AccessTreeNode* parent = nullptr;
    size_t index           = 0; // Index in parent
    utl::small_vector<std::unique_ptr<AccessTreeNode>> children;
};

struct VariableContext {
    explicit VariableContext(Alloca* address, ir::Context& irCtx):
        irCtx(irCtx), baseAlloca(address) {}

    bool buildAccessTree();

    void slice();

    void cleanUnusedLoads();

    void cleanUnusedAddresses(Instruction* address);

    bool buildAccessTreeImpl(AccessTreeNode* parent, Instruction* address);

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
        printAccessTreeImpl(&accessTree, -1);
    }

    void printAccessTreeImpl(AccessTreeNode const* node, int level) {
        if (node != &accessTree) {
            for (int i = 0; i < level; ++i) {
                std::cout << "    ";
            }
            std::cout << node->index;
            if (node->type) {
                std::cout << " [" << node->type->name() << "]";
            }
            std::cout << std::endl;
        }
        for (auto& c: node->children) {
            if (c) {
                printAccessTreeImpl(c.get(), level + 1);
            }
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

    /// The access trees. This only has as many elements the alloca allocates.
    AccessTreeNode accessTree;
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
    return modifiedAny;
}

void VariableContext::slice() {
    addAllocasForLeaves();
    cleanUnusedLoads();
    replaceBySlices();
}

bool VariableContext::buildAccessTree() {
    auto owner = std::make_unique<AccessTreeNode>();
    addressToTreeNodes.insert({ baseAlloca, owner.get() });
    owner->type = baseAlloca->allocatedType();
    accessTree.setChildAt(0, std::move(owner));
    return buildAccessTreeImpl(nullptr, baseAlloca);
}

/// First this gets called with the `alloca` instruction and then recursively
/// with all users that are `gep`'s
bool VariableContext::buildAccessTreeImpl(AccessTreeNode* parent,
                                          Instruction* address) {
    AccessTreeNode* node = nullptr;
    if (parent) {
        SC_ASSERT(!addressToTreeNodes.contains(address),
                  "gep graph should be acyclic");
        auto* gep = cast<GetElementPointer*>(address);
        if (!gep->hasConstantArrayIndex()) {
            return false;
        }
        utl::small_vector<uint16_t> indices = gep->memberIndices();
        if (parent == &accessTree) {
            SC_ASSERT(gep->basePointer() == this->baseAlloca,
                      "We are level 1 so we may have non-zero array index");
            indices.insert(indices.begin(),
                           utl::narrow_cast<uint16_t>(
                               gep->constantArrayIndex()));
        }
        else if (gep->constantArrayIndex() != 0) {
            return false;
        }
        for (size_t i = 0; i < indices.size(); ++i) {
            size_t const index = indices[i];
            node               = parent->childAt(index);
            if (!node) {
                auto owner = std::make_unique<AccessTreeNode>();
                node       = owner.get();
                parent->setChildAt(index, std::move(owner));
            }
            parent = node;
        }
        addressToTreeNodes.insert({ gep, node });
        if (!node->type) {
            node->type = gep->accessedType();
        }
    }
    else {
        SC_ASSERT(isa<Alloca>(address), "");
        node = &accessTree;
    }
    size_t numGeps = 0;
    for (auto* user: address->users()) {
        if (!isa<Load>(user) && !isa<Store>(user) &&
            !isa<GetElementPointer>(user))
        {
            return false;
        }
        if (auto* load = dyncast<Load*>(user)) {
            loads.push_back(load);
            continue;
        }
        auto* gep = dyncast<GetElementPointer*>(user);
        if (!gep) {
            continue;
        }
        ++numGeps;
        if (!buildAccessTreeImpl(node, gep)) {
            return false;
        }
    }
    if (address == baseAlloca && numGeps == 0) {
        return false;
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
    auto users = address->users() | ranges::to<utl::small_vector<Instruction*>>;
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
    std::string result = utl::strcat(original->name(), ".slice");
    for (size_t index: indices) {
        result += utl::strcat("_", index);
    }
    return result;
}

void VariableContext::addAllocasForLeaves() {
    accessTreeLeafWalk(&accessTree,
                       [&](AccessTreeNode* node,
                           std::span<size_t const> indices) {
        SC_ASSERT(node->type, "We need to know what we allocate");
        node->newScalarVar =
            new Alloca(irCtx, node->type, makeName(baseAlloca, indices));
        baseAlloca->parent()->insert(baseAlloca, node->newScalarVar);
    });
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
    auto users = address->users() | ranges::to<utl::small_vector<Instruction*>>;
    for (auto* user: users) {
        auto* bb = user->parent();
        // clang-format off
        visit(*user, utl::overload{
            [&](Load& load) {
                if (node->children.empty()) {
                    load.setAddress(node->newScalarVar);
                    return;
                }
                Value* aggregate = irCtx.undef(load.type());
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf,
                                             std::span<size_t const> indices) {
                    auto* newLoad =
                        new Load(leaf->newScalarVar,
                                 leaf->newScalarVar->allocatedType(),
                                 utl::strcat(load.name(), ".slice"));
                    bb->insert(&load, newLoad);
                    aggregate = new InsertValue(aggregate,
                                                newLoad,
                                                indices,
                                                utl::strcat(load.name(),
                                                            ".slice"));
                    bb->insert(&load, cast<Instruction*>(aggregate));
                });
                replaceValue(&load, aggregate);
                bb->erase(&load);
            },
            [&](Store& store) {
                if (node->children.empty()) {
                    store.setAddress(node->newScalarVar);
                    return;
                }
                auto* storedValue = store.value();
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf,
                                             std::span<size_t const> indices) {
                    auto* slice =
                        new ExtractValue(storedValue,
                                         indices,
                                         std::string(leaf->newScalarVar->name()));
                    store.parent()->insert(&store, slice);
                    auto* newStore = new Store(irCtx, leaf->newScalarVar, slice);
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
    utl::small_vector<size_t> indices;
    auto impl = [&](auto* node, auto impl) -> void {
        bool const addIndex = node->parent != nullptr;
        if (addIndex) {
            indices.push_back(0);
        }
        for (auto& child: node->children) {
            if (child) {
                impl(child.get(), impl);
            }
            if (addIndex) {
                ++indices.back();
            }
        }
        if (addIndex) {
            indices.pop_back();
        }
        if (node->children.empty()) {
            callback(node, indices);
        }
    };
    impl(root, impl);
}

void VariableContext::accessTreePreorderWalk(
    utl::function_view<void(size_t level, AccessTreeNode*)> callback) {
    size_t l  = 0;
    auto impl = [&](auto* node, auto impl) -> void {
        if (!node) {
            return;
        }
        callback(l, node);
        ++l;
        for (auto&& child: node->children) {
            impl(child.get(), impl);
        }
        --l;
    };
    impl(&accessTree, impl);
}

void VariableContext::accessTreePostorderWalk(
    utl::function_view<void(size_t level, AccessTreeNode*)> callback) {
    size_t l  = 0;
    auto impl = [&](auto* node, auto impl) -> void {
        if (!node) {
            return;
        }
        ++l;
        for (auto&& child: node->children) {
            impl(child.get(), impl);
        }
        --l;
        callback(l, node);
    };
    impl(&accessTree, impl);
}
