#include "Opt/Passes.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/hashtable.hpp>
#include <utl/queue.hpp>
#include <utl/vector.hpp>

#include "Common/Dyncast.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/PassRegistry.h"
#include "IR/PointerInfo.h"
#include "IR/Type.h"
#include "Opt/Common.h"

#include "IR/Print.h"

using namespace scatha;
using namespace opt;
using namespace ir;
using namespace ranges::views;

SC_REGISTER_PASS(opt::pointerAnalysis, "ptranalysis", PassCategory::Analysis);

#define INFO_NODE_DEF(X)                                                       \
    X(InfoNode, void, Abstract)                                                \
    X(TreeNode, InfoNode, Concrete)                                            \
    X(LeafNode, InfoNode, Concrete)

namespace {

enum class InfoNodeID {
#define X(Name, ...) Name,
    INFO_NODE_DEF(X)
#undef X
};

struct InfoNode: csp::base_helper<InfoNode, InfoNodeID> {
    using base_helper::base_helper;
};

struct TreeNode: InfoNode {
    TreeNode(): InfoNode(InfoNodeID::TreeNode) {}

    InfoNode* child(size_t index) {
        auto itr = children.find(index);
        return itr != children.end() ? itr->second : nullptr;
    }

    utl::hashmap<size_t, InfoNode*> children;
};

struct LeafNode: InfoNode {
    LeafNode(PointerInfoDesc const& desc):
        InfoNode(InfoNodeID::LeafNode), desc(desc) {}

    PointerInfoDesc desc;
};

} // namespace

#define X(Name, Parent, Corporeality)                                          \
    SC_DYNCAST_DEFINE(Name, InfoNodeID::Name, Parent, Corporeality)
INFO_NODE_DEF(X)
#undef X

namespace {

struct PtrAnalyzeCtx {
    Context& ctx;
    Function& function;

    utl::hashset<Value const*> provenanceVisited;
    utl::hashset<Value const*> escapingVisited;
    utl::hashset<Value const*> escaping;
    utl::hashmap<Value*, InfoNode*> info;
    /// Maps provenances to pointers within the provenance region
    utl::hashmap<Value const*, utl::hashset<Instruction*>> provToOffset;
    utl::hashmap<Value const*, bool> provEscaping;
    utl::small_vector<UniquePtr<InfoNode>> infoNodes;

    PtrAnalyzeCtx(Context& ctx, Function& function):
        ctx(ctx), function(function) {}

    void run();

    void apply();

    bool isProvenanceEscaping(Value const* prov);

    /// Performs a DFS over the operands to analyze provenance
    InfoNode* provenance(Value& inst);
    InfoNode* provenanceImpl(Alloca& inst);
    InfoNode* provenanceImpl(GetElementPointer& gep);
    InfoNode* provenanceImpl(ExtractValue& inst);
    InfoNode* provenanceImpl(InsertValue& inst);
    InfoNode* provenanceImpl(Call& call);
    InfoNode* provenanceImpl(Value&);
    InfoNode* annotateUnknown(Value& value);

    /// Checks whether \p inst escapes its operands
    void analyzeEscapingOperands(Instruction& inst);
    void analyzeEscapingOperandsImpl(Call& call);
    void analyzeEscapingOperandsImpl(Store& store);
    void analyzeEscapingOperandsImpl(Phi& phi);
    void analyzeEscapingOperandsImpl(Instruction&) {}

    /// Performs a DFS over the users to analyze whether pointers escape
    void analyzeEscaping(Value& value);
    void analyzeEscapingImpl(GetElementPointer& inst);
    void analyzeEscapingImpl(InsertValue& inst);
    void analyzeEscapingImpl(ExtractValue& inst);
    void analyzeEscapingImpl(Value&) {}

    void markEscaping(Value* value) { escaping.insert(value); }
    template <typename NodeType>
    NodeType* addNode(UniquePtr<NodeType> node);
    LeafNode* makeLeafNode(PointerInfoDesc const& desc);
    TreeNode* makeTreeNode();
    InfoNode* clone(InfoNode const& node);
    InfoNode* getNode(Value const* value, std::span<size_t const> indices = {});
    InfoNode* setNode(Value* value, InfoNode* node,
                      std::span<size_t const> indices = {});
    void updateProvenanceMap(Value* value, InfoNode const& node);
};

} // namespace

bool opt::pointerAnalysis(Context& ctx, Function& function) {
    PtrAnalyzeCtx(ctx, function).run();
    return false;
}

void PtrAnalyzeCtx::run() {
    auto values = concat(function.parameters() | transform(cast<Value&>),
                         function.instructions());
    for (auto& value: values) {
        provenance(value);
        if (auto* inst = dyncast<Instruction*>(&value)) {
            analyzeEscapingOperands(*inst);
        }
    }
    for (auto& value: values) {
        analyzeEscaping(value);
    }
    apply();
}

void PtrAnalyzeCtx::apply() {
    for (auto& [value, node]: info) {
        auto* leaf = dyncast<LeafNode const*>(node);
        if (!leaf) {
            continue;
        }
        auto desc = leaf->desc;
        desc.nonEscaping = isProvenanceEscaping(desc.provenance.value());
        // clang-format off
        SC_MATCH (*value) {
            [&](Instruction& inst) {
                inst.setPointerInfo(desc);
            },
            [&](Parameter& param) {
                param.amendPointerInfo(desc);
            },
            [](Value const&) {}
        }; // clang-format on
    }
}

bool PtrAnalyzeCtx::isProvenanceEscaping(Value const* prov) {
    auto itr = provEscaping.find(prov);
    if (itr != provEscaping.end()) {
        return itr->second;
    }
    auto& offsets = provToOffset[prov];
    bool result = ranges::all_of(offsets, [&](Instruction const* inst) {
        if (!isa<PointerType>(inst->type())) {
            return true;
        }
        auto* node = dyncast<LeafNode*>(getNode(inst));
        return node && node->desc.nonEscaping;
    });
    provEscaping[prov] = result;
    return result;
}

InfoNode* PtrAnalyzeCtx::provenance(Value& value) {
    if (auto itr = info.find(&value); itr != info.end()) {
        return itr->second;
    }
    if (!provenanceVisited.insert(&value).second) {
        return nullptr;
    }
    return visit(value, [this](auto& value) { return provenanceImpl(value); });
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(Alloca& inst) {
    PointerInfoDesc desc{ .align = (ssize_t)inst.allocatedType()->align(),
                          .validSize = inst.allocatedSize(),
                          .provenance = PointerProvenance::Static(&inst),
                          .staticProvenanceOffset = 0,
                          .guaranteedNotNull = true };
    return setNode(&inst, makeLeafNode(desc));
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(ExtractValue& inst) {
    auto* node = provenance(*inst.baseValue());
    if (!node) {
        return nullptr;
    }
    for (size_t index: inst.memberIndices()) {
        auto* tree = cast<TreeNode*>(node);
        auto itr = tree->children.find(index);
        if (itr == tree->children.end()) {
            return nullptr;
        }
        node = itr->second;
    }
    return setNode(&inst, node);
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(InsertValue& inst) {
    auto* base = provenance(*inst.baseValue());
    auto* value = provenance(*inst.insertedValue());
    if (!base || !value) {
        return nullptr;
    }
    auto* node = setNode(&inst, clone(*base));
    setNode(&inst, value, inst.memberIndices());
    return node;
}

static ssize_t mod(ssize_t a, ssize_t b) {
    SC_ASSERT(b > 0, "divisor must positive to compute modulo");
    ssize_t result = a % b;
    return result < 0 ? result + std::abs(b) : result;
}

static ssize_t computeAlign(ssize_t baseAlign, ssize_t offset) {
    ssize_t r = mod(offset, baseAlign);
    return r == 0 ? baseAlign : r;
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(GetElementPointer& gep) {
    /// GEP operates on pointers, and pointer values must always correspond to
    /// leaf nodes
    auto* base = cast<LeafNode*>(provenance(*gep.basePointer()));
    if (!base) {
        return nullptr;
    }
    auto const& baseDesc = base->desc;
    auto* accType = gep.accessedType();
    auto staticGEPOffset = gep.constantByteOffset();
    ssize_t align = [&] {
        if (staticGEPOffset) {
            return computeAlign(baseDesc.align, *staticGEPOffset);
        }
        return std::min(baseDesc.align, (ssize_t)accType->align());
    }();
    auto validSize = [&]() -> std::optional<size_t> {
        auto validBaseSize = baseDesc.validSize;
        if (!validBaseSize || !staticGEPOffset) return std::nullopt;
        return utl::narrow_cast<size_t>((ssize_t)*validBaseSize -
                                        *staticGEPOffset);
    }();
    auto provenance = baseDesc.provenance;
    auto staticProvOffset = [&]() -> std::optional<ssize_t> {
        auto baseOffset = baseDesc.staticProvenanceOffset;
        if (!baseOffset || !staticGEPOffset) {
            return std::nullopt;
        }
        return *baseOffset + *staticGEPOffset;
    }();
    auto* node =
        makeLeafNode({ .align = align,
                       .validSize = validSize,
                       .provenance = provenance,
                       .staticProvenanceOffset = staticProvOffset,
                       .guaranteedNotNull = baseDesc.guaranteedNotNull });
    return setNode(&gep, node);
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(Call& call) {
    if (isBuiltinCall(&call, svm::Builtin::alloc)) {
        auto* node = makeLeafNode(
            { /// We happen to know that all pointers returned by
              /// `__builtin_alloc` are aligned to 16 byte boundaries
              .align = 16,
              .provenance = PointerProvenance::Static(&call),
              .staticProvenanceOffset = 0,
              .guaranteedNotNull = true });
        return setNode(&call, node, { { 0 } });
    }
    return annotateUnknown(call);
}

InfoNode* PtrAnalyzeCtx::provenanceImpl(Value& value) {
    auto* info = value.pointerInfo();
    if (info && (isa<Global>(value) || isa<Parameter>(value))) {
        return setNode(&value, makeLeafNode(info->getDesc()));
    }
    return annotateUnknown(value);
}

InfoNode* PtrAnalyzeCtx::annotateUnknown(Value& value) {
    bool hasPointers = false;
    auto dfs = [&](auto& dfs, Type const* type) -> InfoNode* {
        // clang-format off
        return SC_MATCH_R (InfoNode*, *type) {
            [&](RecordType const& type) {
                auto* tree = makeTreeNode();
                for (auto [index, elem]: type.elements() | enumerate) {
                    if (auto* node = dfs(dfs, elem)) {
                        tree->children.insert({ (size_t)index, node });
                    }
                }
                return tree;
            },
            [&](PointerType const&) {
                hasPointers = true;
                return makeLeafNode({
                    .provenance = PointerProvenance::Dynamic(&value),
                    .staticProvenanceOffset = 0
                });
            },
            [&](Type const&) { return nullptr; },
        }; // clang-format on
    };
    auto* node = dfs(dfs, value.type());
    if (hasPointers) {
        return setNode(&value, node);
    }
    return nullptr;
}

void PtrAnalyzeCtx::analyzeEscapingOperands(Instruction& inst) {
    visit(inst, [&](auto& inst) { analyzeEscapingOperandsImpl(inst); });
}

void PtrAnalyzeCtx::analyzeEscapingOperandsImpl(Call& call) {
    /// Deallocation is not considered escaping
    if (isBuiltinCall(&call, svm::Builtin::dealloc)) {
        return;
    }
    for (auto* arg: call.arguments()) {
        markEscaping(arg);
    }
}

void PtrAnalyzeCtx::analyzeEscapingOperandsImpl(Store& store) {
    markEscaping(store.value());
}

void PtrAnalyzeCtx::analyzeEscapingOperandsImpl(Phi& phi) {
    // For now we mark all phi operands as escaping until we figure out a way to
    // properly annotate phi'd pointers
    for (auto* op: phi.operands()) {
        markEscaping(op);
    }
}

void PtrAnalyzeCtx::analyzeEscaping(Value& value) {
    if (!escapingVisited.insert(&value).second) {
        return;
    }
    for (auto* user: value.users()) {
        analyzeEscaping(*user);
    }
    auto* node = dyncast<LeafNode*>(getNode(&value));
    if (node && !escaping.contains(&value)) {
        node->desc.nonEscaping = true;
    }
    visit(value, [this](auto& value) { analyzeEscapingImpl(value); });
}

void PtrAnalyzeCtx::analyzeEscapingImpl(GetElementPointer& inst) {
    if (escaping.contains(&inst)) {
        markEscaping(inst.basePointer());
    }
}

void PtrAnalyzeCtx::analyzeEscapingImpl(InsertValue& inst) {
    if (!escaping.contains(&inst)) {
        return;
    }
    for (auto* operand: inst.operands()) {
        markEscaping(operand);
    }
}

void PtrAnalyzeCtx::analyzeEscapingImpl(ExtractValue& inst) {
    if (escaping.contains(&inst)) {
        markEscaping(inst.baseValue());
    }
}

template <typename NodeType>
NodeType* PtrAnalyzeCtx::addNode(UniquePtr<NodeType> node) {
    auto* result = node.get();
    infoNodes.push_back(std::move(node));
    return result;
}

LeafNode* PtrAnalyzeCtx::makeLeafNode(PointerInfoDesc const& desc) {
    return addNode(allocate<LeafNode>(desc));
}

TreeNode* PtrAnalyzeCtx::makeTreeNode() {
    return addNode(allocate<TreeNode>());
}

InfoNode* PtrAnalyzeCtx::clone(InfoNode const& node) {
    // clang-format off
    auto copy = SC_MATCH_R (UniquePtr<InfoNode>, node) {
        [&](TreeNode const& node) {
            auto copy = allocate<TreeNode>();
            for (auto [index, child]: node.children) {
                copy->children.insert({ (size_t)index, clone(*child) });
            }
            return copy;
        },
        [](LeafNode const& node) { return allocate<LeafNode>(node.desc); },
    }; // clang-format on
    return addNode(std::move(copy));
}

InfoNode* PtrAnalyzeCtx::getNode(Value const* value,
                                 std::span<size_t const> indices) {
    auto itr = info.find(value);
    if (itr == info.end()) {
        return nullptr;
    }
    auto* node = itr->second;
    for (size_t index: indices) {
        auto* tree = dyncast<TreeNode*>(node);
        if (!tree) {
            return nullptr;
        }
        node = tree->child(index);
    }
    return node;
}

InfoNode* PtrAnalyzeCtx::setNode(Value* value, InfoNode* node,
                                 std::span<size_t const> indices) {
    if (indices.empty()) {
        info.insert_or_assign(value, node);
        updateProvenanceMap(value, *node);
        return node;
    }
    auto* current = [&]() -> InfoNode* {
        auto itr = info.find(value);
        if (itr != info.end()) {
            return itr->second;
        }
        auto* n = makeTreeNode();
        info.insert({ value, n });
        return n;
    }();
    for (size_t index: indices | drop(1)) {
        auto* tree = cast<TreeNode*>(current);
        auto itr = tree->children.find(index);
        if (itr != tree->children.end()) {
            current = itr->second;
            continue;
        }
        current = makeTreeNode();
        tree->children.insert({ index, current });
    }
    cast<TreeNode*>(current)->children.insert_or_assign(indices.back(), node);
    updateProvenanceMap(value, *node);
    return node;
}

void PtrAnalyzeCtx::updateProvenanceMap(Value* value, InfoNode const& node) {
    auto* inst = dyncast<Instruction*>(value);
    if (!inst) {
        return;
    }
    auto dfs = [&](auto& dfs, InfoNode const& node) -> void {
        // clang-format off
        SC_MATCH (node) {
            [&](TreeNode const& node) {
                for (auto& [index, child]: node.children) {
                    dfs(dfs, *child);
                }
            },
            [&](LeafNode const& node) {
                auto* prov = node.desc.provenance.value();
                provToOffset[prov].insert(inst);
            },
        }; // clang-format on
    };
    dfs(dfs, node);
}
