#include "Opt/SROA.h"

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
    explicit AccessTreeNode(size_t index): idx(index) {}

    AccessTreeNode* at(size_t index) {
        if (index >= children.size()) {
            children.resize(index + 1);
        }
        auto& result = children[index];
        if (!result) {
            result         = std::make_unique<AccessTreeNode>(index);
            result->parent = this;
        }
        return result.get();
    }

    Type const* type     = nullptr;
    Alloca* newScalarVar = nullptr;
    size_t idx;
    AccessTreeNode* parent = nullptr;
    utl::small_vector<std::unique_ptr<AccessTreeNode>> children;
};

struct VariableContext {
    explicit VariableContext(Alloca* address, ir::Context& irCtx):
        irCtx(irCtx), address(address) {}

    bool gatherAndCheckSliceable(Value* = nullptr);

    /// Main algorithm
    void slice();

    /// All 'leaf' GEPs are transformed such that they don't access other GEPs
    /// but the `alloca` directly.
    void simplifyGEPChains();

    void cleanUnusedGEPs();

    void cleanUnusedLoads();

    void buildAccessTree();

    void addAllocasForLeaves();

    void replaceBySlices();

    void replaceBySlicesImpl(Instruction* address, AccessTreeNode* node);

    void accessTreeLeafWalk(
        AccessTreeNode* root,
        utl::function_view<void(AccessTreeNode*, std::span<size_t const>)>
            callback);

    void accessTreePreorderWalk(
        AccessTreeNode* root,
        utl::function_view<void(AccessTreeNode*)> callback);

    AccessTreeNode* rootAt(size_t index) {
        if (accessTreeRoots.size() >= index) {
            accessTreeRoots.resize_emplace(index + 1, 0);
        }
        return &accessTreeRoots[index];
    }

    ir::Context& irCtx;

    /// The address of the structure we are trying to slice.
    Alloca* address;

    /// Every `getelementptr` instruction that directly or indirectly accesses
    /// the structure.
    utl::small_vector<GetElementPointer*> GEPs;

    /// Maps `getelementptr` instructions to their corresponding nodes in the
    /// access tree.
    utl::hashmap<GetElementPointer*, AccessTreeNode*> accessNodeMap;

    /// The access trees. This only has as many elements the alloca allocates.
    utl::small_vector<AccessTreeNode> accessTreeRoots;
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
        if (!varInfo.gatherAndCheckSliceable()) {
            continue;
        }
        if (varInfo.GEPs.empty()) {
            continue;
        }
        varInfo.slice();
        modifiedAny = true;
    }
    return modifiedAny;
}

bool VariableContext::gatherAndCheckSliceable(Value* value) {
    bool const first = !value;
    if (first) {
        value = address;
        if (auto* constCount = dyncast<IntegralConstant*>(address->count())) {
            accessTreeRoots.resize_emplace(constCount->value().to<size_t>(), 0);
        }
    }
    for (auto* user: value->users()) {
        if (!isa<Load>(user) && !isa<Store>(user) &&
            !isa<GetElementPointer>(user))
        {
            return false;
        }
        auto* gep = dyncast<GetElementPointer*>(user);
        if (!gep) {
            continue;
        }
        /// We cannot promote with non-constant gep array index.
        if (!gep->hasConstantArrayIndex()) {
            return false;
        }
        /// And only the first index can non-zero.
        if (!first && gep->constantArrayIndex() != 0) {
            return false;
        }
        GEPs.push_back(gep);
        if (!gatherAndCheckSliceable(gep)) {
            return false;
        }
    }
    return !GEPs.empty();
}

void VariableContext::slice() {
    simplifyGEPChains();
    cleanUnusedGEPs();
    cleanUnusedLoads();
    buildAccessTree();
    addAllocasForLeaves();
    replaceBySlices();
}

/// Transform chains of geps into single geps with multiple indices
void VariableContext::simplifyGEPChains() {
    for (auto* gep: GEPs) {
        /// TODO: Consider if this is really correct
        /// What if a GEP is being loaded from, but also refined by other GEPs?
        bool const allUsersAreLoadsOrStores =
            ranges::all_of(gep->users(), [](User* user) {
                return isa<Load>(user) || isa<Store>(user);
            });
        if (!allUsersAreLoadsOrStores) {
            continue;
        }
        auto* basePtr           = gep->basePointer();
        GetElementPointer* root = gep;
        while (auto* baseGep = dyncast<GetElementPointer*>(basePtr)) {
            gep->setBasePtr(baseGep->basePointer());
            gep->setAccessedType(baseGep->inboundsType());
            for (size_t index:
                 baseGep->memberIndices() | ranges::views::reverse)
            {
                gep->addMemberIndexFront(index);
            }
            basePtr = baseGep->basePointer();
            root    = baseGep;
        }
        if (root != gep) {
            root->setArrayIndex(gep->arrayIndex());
        }
    }
}

void VariableContext::cleanUnusedGEPs() {
    for (auto itr = GEPs.begin(); itr != GEPs.end();) {
        auto* gep = *itr;
        if (gep->users().empty()) {
            gep->parent()->erase(gep);
            itr = GEPs.erase(itr);
        }
        else {
            ++itr;
        }
    }
}

void VariableContext::cleanUnusedLoads() {
    auto impl = [](Value* address) {
        utl::small_vector<Load*> unusedLoads;
        for (auto* user: address->users()) {
            auto* load = dyncast<Load*>(user);
            if (!load) {
                continue;
            }
            if (load->users().empty()) {
                unusedLoads.push_back(load);
            }
        }
        for (auto* load: unusedLoads) {
            load->parent()->erase(load);
        }
    };
    for (auto* gep: GEPs) {
        impl(gep);
    }
    impl(address);
}

void VariableContext::buildAccessTree() {
    for (auto* gep: GEPs) {
        AccessTreeNode* node = rootAt(gep->constantArrayIndex());
        for (size_t index: gep->memberIndices()) {
            node = node->at(index);
        }
        node->type         = gep->accessedType();
        accessNodeMap[gep] = node;
    }
    /// Complete the tree
    for (auto& root: accessTreeRoots) {
        root.type = address->allocatedType();
        accessTreePreorderWalk(&root, [&](AccessTreeNode* node) {
            /// If `node->type` is set, then we know this node is directly
            /// accessed. That means we need to make sure all children are
            /// present.
            if (!node->children.empty() && node->type &&
                isa<StructureType>(node->type))
            {
                auto* sType = cast<StructureType const*>(node->type);
                for (auto [index, memType]:
                     sType->members() | ranges::views::enumerate)
                {
                    auto* childNode = node->at(index);
                    childNode->type = memType;
                }
            }
        });
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
    for (auto& root: accessTreeRoots) {
        accessTreeLeafWalk(&root,
                           [&](AccessTreeNode* node,
                               std::span<size_t const> indices) {
            SC_ASSERT(node->type, "");
            node->newScalarVar =
                new Alloca(irCtx, node->type, makeName(address, indices));
            address->parent()->insert(address, node->newScalarVar);
        });
    }
}

void VariableContext::replaceBySlices() {
    for (auto* gep: GEPs) {
        AccessTreeNode* node = accessNodeMap[gep];
        replaceBySlicesImpl(gep, node);
    }
    /// This handles the non-array case and does not hurt in the array case.
    replaceBySlicesImpl(address, rootAt(0));
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
            [&](Instruction& inst) { SC_UNREACHABLE(); },
        }); // clang-format on
    }
    SC_ASSERT(address->users().empty(), "");
    address->parent()->erase(address);
}

void VariableContext::accessTreeLeafWalk(
    AccessTreeNode* root,
    utl::function_view<void(AccessTreeNode*, std::span<size_t const>)>
        callback) {
    utl::small_vector<size_t> indices;
    auto impl = [&](auto* node, auto impl) -> void {
        indices.push_back(0);
        for (auto&& child: node->children) {
            impl(child.get(), impl);
            ++indices.back();
        }
        indices.pop_back();
        if (node->children.empty()) {
            callback(node, indices);
        }
    };
    impl(root, impl);
}

void VariableContext::accessTreePreorderWalk(
    AccessTreeNode* root, utl::function_view<void(AccessTreeNode*)> callback) {
    auto impl = [&](auto* node, auto impl) -> void {
        callback(node);
        for (auto&& child: node->children) {
            impl(child.get(), impl);
        }
    };
    impl(root, impl);
}
