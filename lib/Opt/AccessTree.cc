#include "Opt/AccessTree.h"

#include <iostream>

#include "Common/Ranges.h"
#include "Common/TreeFormatter.h"
#include "IR/CFG/Value.h"
#include "IR/Print.h"

using namespace scatha;
using namespace opt;

AccessTree* AccessTree::sibling(ssize_t offset) {
    if (!parent()) {
        return nullptr;
    }
    ssize_t sIndex = utl::narrow_cast<ssize_t>(*this->index()) + offset;
    size_t index = static_cast<size_t>(sIndex);
    SC_ASSERT(sIndex >= 0, "");
    if (index >= parent()->numChildren()) {
        parent()->_children.resize(index + 1);
    }
    auto& result = parent()->_children[index];
    if (!result) {
        SC_ASSERT(isArrayNode(), "");
        result = std::make_unique<AccessTree>(type(),
                                              parent(),
                                              index,
                                              /* isArrayNode = */ true);
    }
    return result.get();
}

AccessTree* AccessTree::addArrayChild(size_t index) {
    if (isArrayNode()) {
        return sibling(utl::narrow_cast<ssize_t>(index));
    }
    if (_children.size() <= index) {
        _children.resize(index + 1);
    }
    auto& child = _children[index];
    if (!child) {
        child = std::make_unique<AccessTree>(type(),
                                             this,
                                             index,
                                             /* isArrayNode = */ true);
    }
    return child.get();
}

void AccessTree::fanOut() {
    // clang-format off
    SC_MATCH (*type()) {
        [&](ir::StructType const& sType) {
            if (_children.size() < sType.members().size()) {
                _children.resize(sType.members().size());
            }
            for (auto [index, t]: sType.members() | ranges::views::enumerate) {
                auto& child = _children[index];
                if (!child) {
                    child = std::make_unique<AccessTree>(t, this, index);
                }
            }
        },
        [&](ir::ArrayType const& aType) {
            if (!_children.empty()) {
                SC_ASSERT(_children.size() == aType.count(), "");
                return;
            }
            auto* t = aType.elementType();
            for (size_t index = 0; index < aType.count(); ++index) {
                _children.push_back(std::make_unique<AccessTree>(t, this, index, /* isArrayNode = */ true));
            }
        },
        [&](ir::Type const&) {},
    }; // clang-format on
}

AccessTree* AccessTree::addSingleChild(size_t index) {
    auto* sType = dyncast<ir::StructType const*>(_type);
    SC_ASSERT(sType && index < sType->members().size(), "Invalid index");
    if (_children.empty()) {
        _children.resize(sType->members().size());
    }
    auto& child = _children[index];
    if (!child) {
        child =
            std::make_unique<AccessTree>(sType->memberAt(index), this, index);
    }
    return child.get();
}

std::unique_ptr<AccessTree> AccessTree::clone() {
    auto result = std::make_unique<AccessTree>(type());
    result->_children.resize(_children.size());
    result->_index = _index;
    result->_value = _value;
    for (auto&& [child, resChild]:
         ranges::views::zip(_children, result->_children))
    {
        if (child) {
            resChild = child->clone();
            resChild->_parent = result.get();
        }
    }
    return result;
}

void AccessTree::print() const { print(std::cout); }

static void printImpl(AccessTree const* node,
                      std::ostream& str,
                      TreeFormatter& formatter) {
    str << formatter.beginLine();
    if (auto index = node->index()) {
        str << *index << " : ";
    }
    if (auto* value = node->value()) {
        str << toString(node->value());
    }
    else {
        str << "<No value>";
    }
    if (auto* type = node->type()) {
        str << " " << type->name();
    }
    else {
        str << " <No type>";
    }
    if (node->isArrayNode()) {
        str << " [DynArrayNode]";
    }
    str << std::endl;
    auto children = node->children() |
                    ranges::views::filter([](auto* p) { return !!p; }) |
                    ToSmallVector<>;
    for (auto [index, child]: children | ranges::views::enumerate) {
        formatter.push(index != children.size() - 1 ? Level::Child :
                                                      Level::LastChild);
        printImpl(child, str, formatter);
        formatter.pop();
    }
}

void AccessTree::print(std::ostream& str) const {
    TreeFormatter formatter;
    printImpl(this, str, formatter);
}
