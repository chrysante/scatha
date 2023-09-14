#include "Opt/MemberTree.h"

#include <iostream>

#include <range/v3/view.hpp>

#include "Common/TreeFormatter.h"
#include "IR/Print.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;
using namespace opt;

MemberTree MemberTree::compute(ir::Type const* type) {
    MemberTree result;
    result._root = result.computeDFS(type, 0, 0);
    return result;
}

MemberTree::Node* MemberTree::computeDFS(ir::Type const* type,
                                         size_t index,
                                         size_t offset) {
    nodes.push_back(std::make_unique<Node>(
        Payload{ index, type, offset, offset + type->size() }));
    auto* result = nodes.back().get();
    // clang-format off
    SC_MATCH (*type) {
        [&](ir::StructType const& type) {
            for (size_t i = 0; i < type.members().size(); ++i) {
                auto* node = computeDFS(type.memberAt(i), i, offset + type.memberOffsetAt(i));
                result->addChild(node);
            }
        },
        [&](ir::ArrayType const& type) {
            auto* elemType = type.elementType();
            for (size_t i = 0; i < type.count(); ++i) {
                auto* node = computeDFS(elemType, i, offset + i * elemType->size());
                result->addChild(node);
            }
        },
        [&](ir::Type const& type) {},
    }; // clang-format on
    return result;
}

void opt::print(MemberTree const& tree) { print(tree, std::cout); }

static void printImpl(MemberTree::Node const* node,
                      std::ostream& str,
                      TreeFormatter& formatter) {
    str << formatter.beginLine() << node->index() << ": " << *node->type()
        << " [" << node->begin() << ", " << node->end() << ")\n";
    auto children = node->children();
    for (auto [index, child]: children | ranges::views::enumerate) {
        formatter.push(index != children.size() - 1 ? Level::Child :
                                                      Level::LastChild);
        printImpl(child, str, formatter);
        formatter.pop();
    }
}

void opt::print(MemberTree const& tree, std::ostream& str) {
    TreeFormatter formatter;
    printImpl(tree.root(), str, formatter);
}
