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
    /// The IR context
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
    std::unique_ptr<AccessTree> accessTree;

    /// We keep track of the added allocas to prune them in the end
    utl::small_vector<Alloca*> addedAllocas;

    /// Construct a variable context
    explicit VariableContext(Alloca* address, ir::Context& irCtx):
        irCtx(irCtx), baseAlloca(address) {}

    /// First step in the algorithm. If this returns `false` then slicing is not
    /// possible. The access tree is built by looking at all the GEPs that
    /// directly or transitively access this alloca.
    bool buildAccessTree();

    /// Helper functions for `buildAccessTree()`
    bool buildAccessTreeVisitUsers(AccessTree* parent, Instruction* address);
    bool buildAccessTreeImpl(AccessTree* parent, GetElementPointer* address);
    bool addArrayNodes(AccessTree* firstElemNode, Type const* type);

    /// The the allocation according to the access tree
    void slice();

    /// Recursively walks the access tree and inserts alloca instructions for
    /// the leaf nodes
    void addAllocasForLeaves();
    void addAllocasForLeavesImpl(AccessTree* node);

    /// Replaces loads by creating single loads to each leaf in the subtree and
    /// combining the individual values with `InsertValue` instructions.
    /// Replaces stores analogously using `ExtractValue` instructions
    void replaceBySlices();
    Value* loadSlices(Type const* type,
                      std::string name,
                      Instruction* insertBefore,
                      AccessTree* node);
    void replaceBySlicesImpl(Load* load, AccessTree* node);
    void storeSlices(Instruction* insertBefore,
                     Value* storedValue,
                     AccessTree* node);
    void replaceBySlicesImpl(Store* store, AccessTree* node);
    void replaceBySlicesImpl(GetElementPointer* gep, AccessTree* node);
    void replaceBySlicesImpl(Instruction* inst, AccessTree* node) {
        SC_UNREACHABLE();
    }

    /// This is an intermediate step called during slicing after building the
    /// access tree
    void cleanUnusedLoads();

    /// This is called in the end after slicing and cleans up all unnecessarily
    /// added instructions
    void clean();

    /// Called by `clean()`. Recursively walks all the GEP instructions that
    /// access this alloca (in post-order) and erases any instructions that are
    /// not used
    void cleanUnusedGEPs(Instruction* address);

    void accessTreeLeafWalk(
        AccessTree* root,
        utl::function_view<void(AccessTree*, std::span<size_t const>)>
            callback);
};

} // namespace

bool opt::sroa(ir::Context& irCtx, ir::Function& function) {
    auto allocas =
        function.entry() | TakeAddress | Filter<Alloca> | ToSmallVector<>;
    bool modifiedAny = false;
    for (auto* allocaInst: allocas) {
        VariableContext varInfo(allocaInst, irCtx);
        if (!varInfo.buildAccessTree()) {
            continue;
        }
        varInfo.slice();
        varInfo.clean();
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

/// \Returns the element type if \p type is an array type, otherwise returns \p
/// type
static Type const* stripArrayType(Type const* type) {
    if (auto* arrayType = dyncast<ArrayType const*>(type)) {
        return arrayType->elementType();
    }
    return type;
}

bool VariableContext::buildAccessTree() {
    accessTree = std::make_unique<AccessTree>(stripArrayType(
                                                  baseAlloca->allocatedType()),
                                              nullptr,
                                              0,
                                              true);
    auto* firstElem = accessTree->addArrayChild(0);
    addressToTreeNodes.insert({ baseAlloca, firstElem });
    if (!buildAccessTreeVisitUsers(firstElem, baseAlloca)) {
        return false;
    }
    return true;
}

bool VariableContext::buildAccessTreeVisitUsers(AccessTree* node,
                                                Instruction* address) {
    /// Flag to check wether there is anything to destructure
    bool haveStructure = false;
    for (auto* inst: address->users()) {
        // clang-format off
        bool flag = SC_MATCH (*inst) {
            [&](Load& load) {
                loads.push_back(&load);
                haveStructure |= addArrayNodes(node, load.type());
                return true;
            },
            [&](Store const& store) {
                /// For `store`s we also need to check if we are actually the
                /// address and not the value argument.
                if (store.value() == address) {
                    return false;
                }
                haveStructure |= addArrayNodes(node, store.value()->type());
                return true;
            },
            [&](GetElementPointer& gep) {
                haveStructure = true;
                return buildAccessTreeImpl(node, &gep);
            },
            [](Instruction const&) {
                /// If any user is not a `load`, `store` or `getelementptr`
                /// instruction, we don't consider this `alloca`
                return false;
            },
            
        }; // clang-format on
        if (!flag) {
            return false;
        }
    }
    if (address == baseAlloca) {
        return haveStructure;
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
    size_t arrayIndex = gep->constantArrayIndex();
    if (node->parent()->isArrayNode()) {
        node = node->sibling(utl::narrow_cast<ssize_t>(arrayIndex));
    }
    else {
        node = node->addArrayChild(arrayIndex);
    }
    for (size_t index: gep->memberIndices()) {
        node->fanOut();
        node = node->childAt(index);
    }
    addressToTreeNodes.insert({ gep, node });
    return buildAccessTreeVisitUsers(node, gep);
}

bool VariableContext::addArrayNodes(AccessTree* firstElemNode,
                                    Type const* type) {
    auto* arrayType = dyncast<ArrayType const*>(type);
    if (!arrayType) {
        return false;
    }
    auto* parent = firstElemNode->parent();
    size_t nodeIndex = *firstElemNode->index();
    /// We start from 1 because node 0 already exists (it is `firstElemNode`)
    for (size_t i = 1; i < arrayType->count(); ++i) {
        parent->addArrayChild(nodeIndex + i);
    }
    return true;
}

void VariableContext::addAllocasForLeaves() {
    addAllocasForLeavesImpl(accessTree.get());
}

void VariableContext::addAllocasForLeavesImpl(AccessTree* node) {
    if (node->children().empty()) {
        auto* newScalar =
            new Alloca(irCtx, node->type(), std::string(baseAlloca->name()));
        node->setValue(newScalar);
        baseAlloca->parent()->insert(baseAlloca, newScalar);
        addedAllocas.push_back(newScalar);
    }
    for (auto* child: node->children()) {
        if (child) {
            addAllocasForLeavesImpl(child);
        }
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

Value* VariableContext::loadSlices(Type const* type,
                                   std::string name,
                                   Instruction* insertBefore,
                                   AccessTree* node) {
    auto* bb = insertBefore->parent();
    Value* aggregate = irCtx.undef(type);
    accessTreeLeafWalk(node,
                       [&](AccessTree* leaf, std::span<size_t const> indices) {
        auto* newLoad =
            new Load(leaf->value(), getAlloca(leaf)->allocatedType(), name);
        bb->insert(insertBefore, newLoad);
        if (!indices.empty()) {
            aggregate = new InsertValue(aggregate, newLoad, indices, name);
            bb->insert(insertBefore, cast<Instruction*>(aggregate));
        }
        else {
            aggregate = newLoad;
        }
    });
    return aggregate;
}

void VariableContext::replaceBySlicesImpl(Load* load, AccessTree* node) {
    auto* bb = load->parent();
    auto* loadedType = load->type();
    auto* arrayType = dyncast<ArrayType const*>(loadedType);
    auto name = std::string(load->name());
    Value* newValue = nullptr;
    if (!arrayType) {
        newValue = loadSlices(loadedType, name, load, node);
    }
    else {
        auto* elemType = arrayType->elementType();
        SC_ASSERT(arrayType && elemType == node->type(), "");
        newValue = irCtx.undef(loadedType);
        for (size_t i = 0; i < arrayType->count(); ++i) {
            auto* aggregate =
                new InsertValue(newValue,
                                loadSlices(elemType, name, load, node),
                                std::array{ i },
                                name);
            bb->insert(load, aggregate);
            newValue = aggregate;
            node = node->rightSibling();
        }
    }
    load->replaceAllUsesWith(newValue);
    bb->erase(load);
}

void VariableContext::storeSlices(Instruction* insertBefore,
                                  Value* storedValue,
                                  AccessTree* node) {
    auto* bb = insertBefore->parent();
    accessTreeLeafWalk(node,
                       [&](AccessTree* leaf, std::span<size_t const> indices) {
        auto* value = storedValue;
        if (!indices.empty()) {
            auto* slice = new ExtractValue(storedValue,
                                           indices,
                                           std::string(leaf->value()->name()));
            bb->insert(insertBefore, slice);
            value = slice;
        }
        auto* newStore = new Store(irCtx, leaf->value(), value);
        bb->insert(insertBefore, newStore);
    });
}

void VariableContext::replaceBySlicesImpl(Store* store, AccessTree* node) {
    auto* bb = store->parent();
    auto* storedValue = store->value();
    auto* storedType = storedValue->type();
    auto* arrayType = dyncast<ArrayType const*>(storedType);
    if (!arrayType) {
        storeSlices(store, storedValue, node);
    }
    else {
        SC_ASSERT(arrayType && arrayType->elementType() == node->type(), "");
        for (size_t i = 0; i < arrayType->count(); ++i) {
            auto* slice =
                new ExtractValue(storedValue,
                                 std::array{ i },
                                 utl::strcat(storedValue->name(), ".", i));
            bb->insert(store, slice);
            storeSlices(store, slice, node);
            node = node->rightSibling();
        }
    }
    bb->erase(store);
}

void VariableContext::replaceBySlicesImpl(GetElementPointer* gep,
                                          AccessTree* node) {}

void VariableContext::cleanUnusedLoads() {
    for (auto& inst: loads) {
        if (inst->users().empty()) {
            inst->parent()->erase(inst);
            inst = nullptr;
        }
    }
}

void VariableContext::clean() {
    cleanUnusedGEPs(baseAlloca);
    for (auto* inst: addedAllocas) {
        if (!inst->isUsed()) {
            inst->parent()->erase(inst);
        }
    }
}

void VariableContext::cleanUnusedGEPs(Instruction* address) {
    auto users = address->users() | Filter<GetElementPointer> | ToSmallVector<>;
    for (auto* gep: users) {
        cleanUnusedGEPs(gep);
    }
    if (address->users().empty()) {
        address->parent()->erase(address);
    }
}

void VariableContext::accessTreeLeafWalk(
    AccessTree* root,
    utl::function_view<void(AccessTree*, std::span<size_t const>)> callback) {
    SC_ASSERT(root, "");
    utl::small_vector<size_t> indices;
    auto impl = [&](auto* node, auto impl) -> void {
        if (node->hasChildren()) {
            for (auto [index, child]:
                 node->children() | ranges::views::enumerate)
            {
                if (!child) {
                    continue;
                }
                if (!node->isArrayNode()) {
                    indices.push_back(index);
                    impl(child, impl);
                    indices.pop_back();
                }
                else {
                    impl(child, impl);
                }
            }
        }
        else {
            callback(node, indices);
        }
    };
    impl(root, impl);
}
