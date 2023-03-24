#include "Opt/SROA.h"

#include <memory>

#include <utl/function_view.hpp>
#include <utl/vector.hpp>

#include "Common/Allocator.h"
#include "IR/CFG.h"
#include "IR/Context.h"
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

struct VariableInfo {
    void cleanUnusedLoads(Value* address);

    void buildAccessTree();

    void addAllocasForLeaves(AccessTreeNode*) const;

    void replaceBySlices(Instruction* address);

    void accessTreeLeafWalk(
        AccessTreeNode* root,
        utl::function_view<void(AccessTreeNode*, std::span<size_t const>)>
            callback);

    Alloca* address;
    ir::Context* irCtx;
    utl::small_vector<GetElementPointer*> geps;
    utl::hashmap<Value*, AccessTreeNode*> accessNodeMap;
    AccessTreeNode accessTreeRoot{ 0 };
};

struct SROAContext {
    SROAContext(ir::Context& irCtx, ir::Function& function):
        irCtx(irCtx), function(function) {}

    bool run();

    void gather();

    void slice(VariableInfo& varInfo);

    ir::Context& irCtx;
    ir::Function& function;
    utl::small_vector<VariableInfo> variables;
};

} // namespace

bool opt::sroa(ir::Context& irCtx, ir::Function& function) {
    SROAContext ctx(irCtx, function);
    return ctx.run();
}

static bool isSROAPromotable(Value* value,
                             utl::vector<GetElementPointer*>& geps) {
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
        /// We cannot promote with non-constant or non-zero gep array index.
        if (!gep->hasConstantArrayIndex() || gep->constantArrayIndex() != 0) {
            return false;
        }
        geps.push_back(gep);
        if (!isSROAPromotable(gep, geps)) {
            return false;
        }
    }
    return true;
}

#include <iostream>

bool SROAContext::run() {
    gather();

    for (auto& varInfo: variables) {
        slice(varInfo);
    }

    return false;
}

void SROAContext::gather() {
    for (auto& inst: function.entry()) {
        auto* address = dyncast<Alloca*>(&inst);
        if (!address) {
            continue;
        }
        VariableInfo varInfo{ address, &irCtx };
        if (!isSROAPromotable(address, varInfo.geps)) {
            continue;
        }
        if (varInfo.geps.empty()) {
            continue;
        }
        variables.push_back(std::move(varInfo));
    }
}

void SROAContext::slice(VariableInfo& varInfo) {

    /// Transform chains of geps into single geps with multiple indices

    for (auto* gep: varInfo.geps) {
        if (ranges::all_of(gep->users(), [](User* user) {
                return isa<Load>(user) || isa<Store>(user);
            }))
        {
            auto* basePtr = gep->basePointer();
            while (auto* baseGep = dyncast<GetElementPointer*>(basePtr)) {
                gep->setBasePtr(baseGep->basePointer());
                gep->setAccessedType(baseGep->inboundsType());
                SC_ASSERT(baseGep->memberIndices().size() == 1, "");
                gep->addMemberIndexFront(baseGep->memberIndices().front());
                basePtr = baseGep->basePointer();
            }
        }
    }

    for (auto itr = varInfo.geps.begin(); itr != varInfo.geps.end();) {
        auto* gep = *itr;
        if (gep->users().empty()) {
            gep->parent()->erase(gep);
            itr = varInfo.geps.erase(itr);
        }
        else {
            ++itr;
        }
    }

    for (auto* gep: varInfo.geps) {
        varInfo.cleanUnusedLoads(gep);
    }
    varInfo.cleanUnusedLoads(varInfo.address);

    varInfo.buildAccessTree();

    varInfo.addAllocasForLeaves(&varInfo.accessTreeRoot);

    //    return;

    for (auto* gep: varInfo.geps) {
        varInfo.replaceBySlices(gep);
    }
    varInfo.replaceBySlices(varInfo.address);
}

void VariableInfo::cleanUnusedLoads(Value* address) {
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
}

void VariableInfo::buildAccessTree() {
    for (auto* gep: geps) {
        AccessTreeNode* node = &accessTreeRoot;
        for (size_t index: gep->memberIndices()) {
            node = node->at(index);
        }
        node->type         = gep->accessedType();
        accessNodeMap[gep] = node;
    }
    accessNodeMap[address] = &accessTreeRoot;
    accessTreeRoot.type    = address->allocatedType();

    /// complete the tree
    auto walk = [&](AccessTreeNode* node, auto& walk) -> void {
        /// If `node->type` is set, then we know this node is directly accessed.
        /// That means we need to make sure all children are present.
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
        for (auto& child: node->children) {
            walk(child.get(), walk);
        }
    };
    walk(&accessTreeRoot, walk);

    // Print access tree...
    std::cout << address->name() << ": " << address->type()->name() << "\n";
    auto printTree = [indent = 0](AccessTreeNode* node,
                                  auto& printTree) mutable -> void {
        for (int i = 0; i < indent; ++i)
            std::cout << "  ";
        std::cout << node->idx;
        if (node->type) {
            std::cout << ": " << node->type->name();
        }
        std::cout << "\n";
        ++indent;
        for (auto& child: node->children) {
            printTree(child.get(), printTree);
        }
        --indent;
    };
    for (auto& child: accessTreeRoot.children) {
        printTree(child.get(), printTree);
    }
}

void VariableInfo::addAllocasForLeaves(AccessTreeNode* node) const {
    for (auto& child: node->children) {
        addAllocasForLeaves(child.get());
    }
    if (node->children.empty()) {
        SC_ASSERT(node->type, "");
        node->newScalarVar = new Alloca(*irCtx,
                                        node->type,
                                        utl::strcat("slice.", address->name()));
        address->parent()->insert(address, node->newScalarVar);
    }
}

void VariableInfo::replaceBySlices(Instruction* address) {
    AccessTreeNode* node = accessNodeMap[address];
    /// Make a copy of the user list because we edit the user list during
    /// traversal.
    auto users = address->users() | ranges::to<utl::small_vector<Instruction*>>;
    // clang-format off
    for (auto* user: users) {
        auto* bb = user->parent();
        visit(*user, utl::overload{
            [&](Load& load) {
                if (node->children.empty()) {
                    load.setAddress(node->newScalarVar);
                    return;
                }
                Value* aggregate = irCtx->undef(load.type());
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf, std::span<size_t const> indices) {
                    auto* newLoad = new Load(leaf->newScalarVar, leaf->newScalarVar->allocatedType(), utl::strcat("slice.", load.name()));
                    bb->insert(&load, newLoad);
                    aggregate = new InsertValue(aggregate, newLoad, indices, utl::strcat("slice.", load.name()));
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
                accessTreeLeafWalk(node, [&](AccessTreeNode* leaf, std::span<size_t const> indices) {
                    auto* slice = new ExtractValue(storedValue, indices, std::string(leaf->newScalarVar->name()));
                    store.parent()->insert(&store, slice);
                    auto* newStore = new Store(*irCtx, leaf->newScalarVar, slice);
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

void VariableInfo::accessTreeLeafWalk(
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
