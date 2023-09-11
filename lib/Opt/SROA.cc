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

struct VariableContext {
    explicit VariableContext(Alloca* address, ir::Context& irCtx):
        irCtx(irCtx), baseAlloca(address) {}

    bool buildAccessTree();

    void slice();

    void cleanUnusedLoads();

    void cleanUnusedAddresses(Instruction* address);

    /// Build the access tree with using all the GEPs that directly or
    /// indirectly access this alloca.
    bool buildAccessTreeImpl(AccessTree* parent, GetElementPointer* address);

    bool buildAccessTreeVisitUsers(AccessTree* parent, Instruction* address);

    void addAllocasForLeaves();

    void replaceBySlices();

    void replaceBySlicesImpl(Load* load, AccessTree* node);
    void replaceBySlicesImpl(Store* store, AccessTree* node);
    void replaceBySlicesImpl(GetElementPointer* gep, AccessTree* node);
    void replaceBySlicesImpl(Instruction* inst, AccessTree* node) {
        SC_UNREACHABLE();
    }

    AccessTree* sibling(AccessTree* node, ssize_t offset) {
        if (node->parent()) {
            return node->sibling(offset);
        }
        auto itr =
            ranges::find_if(accessForest,
                            [&](auto& root) { return root.get() == node; }) +
            offset;
        SC_ASSERT(itr < accessForest.end(), "Invalid node or offset");
        return itr->get();
    }

    void accessTreeLeafWalk(
        AccessTree* root,
        utl::function_view<void(AccessTree*, std::span<size_t const>)>
            callback);

    void accessTreePreorderWalk(
        utl::function_view<void(size_t level, AccessTree*)> callback);

    void accessTreePostorderWalk(
        utl::function_view<void(size_t level, AccessTree*)> callback);

    void printAccessTree() {
        std::cout << "%" << baseAlloca->name() << ":" << std::endl;
        for (auto& node: accessForest) {
            printAccessTreeImpl(node.get(), 0);
        }
    }

    void printAccessTreeImpl(AccessTree const* node, int level) {
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
    utl::hashmap<Instruction*, AccessTree*> addressToTreeNodes;

    /// While building the access tree we collect all loads to later erase all
    /// unused loads
    utl::small_vector<Load*, 32> loads;

    /// The access trees. This has as many elements as there are array accesses
    /// by GEP instructions.
    utl::small_vector<std::unique_ptr<AccessTree>> accessForest;
};

} // namespace

bool opt::sroa(ir::Context& irCtx, ir::Function& function) {
    auto allocas =
        function.entry() | TakeAddress | Filter<Alloca> | ToSmallVector<>;
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

static ir::Alloca* getAlloca(AccessTree* node) {
    return cast<ir::Alloca*>(node->value());
}

void VariableContext::slice() {
    addAllocasForLeaves();
    cleanUnusedLoads();
    replaceBySlices();
}

ir::Type const* getRootType(ir::Alloca const* baseAlloca) {
    auto* allocated = baseAlloca->allocatedType();
    if (auto* array = dyncast<ir::ArrayType const*>(allocated)) {
        return array->elementType();
    }
    return allocated;
}

bool VariableContext::buildAccessTree() {
    SC_ASSERT(accessForest.empty(), "");

    accessForest.push_back(
        std::make_unique<AccessTree>(getRootType(baseAlloca)));
#warning Should this not be accessForest.back()??
    auto* root = accessForest.front().get();
    addressToTreeNodes.insert({ baseAlloca, root });
    if (!buildAccessTreeVisitUsers(nullptr, baseAlloca)) {
        return false;
    }
    return true;
}

/// First this gets called with the `alloca` instruction and then recursively
/// with all users that are `gep`'s
bool VariableContext::buildAccessTreeImpl(AccessTree* node,
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
                std::make_unique<AccessTree>(gep->inboundsType());
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

bool VariableContext::buildAccessTreeVisitUsers(AccessTree* node,
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
                           [&](AccessTree* node,
                               std::span<size_t const> indices) {
            SC_ASSERT(node->type(), "We need to know what we allocate");
            auto* newScalar =
                new Alloca(irCtx, node->type(), makeName(baseAlloca, indices));
            node->setValue(newScalar);
            baseAlloca->parent()->insert(baseAlloca, newScalar);
        });
    }
}

void VariableContext::replaceBySlices() {
    for (auto [addr, node]: addressToTreeNodes) {
        /// Make a copy of the user list because we edit the user list during
        /// traversal.
        for (auto* inst: addr->users() | ToSmallVector<>) {
            visit(*inst, [this, node = node](auto& inst) {
                replaceBySlicesImpl(&inst, node);
            });
        }
    }
}

void VariableContext::replaceBySlicesImpl(Load* load, AccessTree* node) {
    if (!node->hasChildren()) {
        load->setAddress(node->value());
        return;
    }
    auto* bb = load->parent();
    Value* aggregate = irCtx.undef(load->type());
    accessTreeLeafWalk(node,
                       [&](AccessTree* leaf, std::span<size_t const> indices) {
        auto* newLoad = new Load(leaf->value(),
                                 getAlloca(leaf)->allocatedType(),
                                 std::string(load->name()));
        bb->insert(load, newLoad);
        aggregate = new InsertValue(aggregate,
                                    newLoad,
                                    indices,
                                    std::string(load->name()));
        bb->insert(load, cast<Instruction*>(aggregate));
    });
    load->replaceAllUsesWith(aggregate);
    bb->erase(load);
}

void VariableContext::replaceBySlicesImpl(Store* store, AccessTree* node) {
    auto* bb = store->parent();
    auto* storedValue = store->value();
    if (!node->hasChildren()) {
        auto* type = storedValue->type();
        if (auto* arrayType = dyncast<ir::ArrayType const*>(type)) {
            for (size_t i = 0; i < arrayType->count(); ++i) {
                auto* sibling =
                    this->sibling(node, utl::narrow_cast<ssize_t>(i));
                auto* slice = new ExtractValue(storedValue,
                                               std::array{ i },
                                               std::string("sroa.ev"));
                bb->insert(store, slice);
                auto* newStore = new Store(irCtx, sibling->value(), slice);
                bb->insert(store, newStore);
            }
            bb->erase(store);
        }
        else {
            store->setAddress(node->value());
        }
        return;
    }
    accessTreeLeafWalk(node,
                       [&](AccessTree* leaf, std::span<size_t const> indices) {
        auto* slice = new ExtractValue(storedValue,
                                       indices,
                                       std::string(leaf->value()->name()));
        bb->insert(store, slice);
        auto* newStore = new Store(irCtx, leaf->value(), slice);
        bb->insert(store, newStore);
    });
    bb->erase(store);
}

void VariableContext::replaceBySlicesImpl(GetElementPointer* gep,
                                          AccessTree* node) {}

void VariableContext::accessTreeLeafWalk(
    AccessTree* root,
    utl::function_view<void(AccessTree*, std::span<size_t const>)> callback) {
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
    utl::function_view<void(size_t level, AccessTree*)> callback) {
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
    utl::function_view<void(size_t level, AccessTree*)> callback) {
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
