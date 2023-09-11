#include "Opt/AccessTree.h"

#include <iostream>

#include "IR/CFG/Value.h"

using namespace scatha;
using namespace opt;

AccessTree const* AccessTree::sibling(ssize_t offset) const {
    if (!parent()) {
        return nullptr;
    }
    ssize_t sIndex = utl::narrow_cast<ssize_t>(*this->index()) + offset;
    size_t index = static_cast<size_t>(sIndex);
    if (sIndex < 0 || index >= parent()->numChildren()) {
        return nullptr;
    }
    return parent()->childAt(index);
}

void AccessTree::fanOut() {
    auto* sType = dyncast<ir::StructType const*>(type());
    if (!sType) {
        return;
    }
    if (!_children.empty()) {
        SC_ASSERT(_children.size() == sType->members().size(), "");
        return;
    }
    for (auto [index, t]: sType->members() | ranges::views::enumerate) {
        _children.push_back(std::make_unique<AccessTree>(t));
        _children.back()->_index = index;
    }
}

AccessTree* AccessTree::addSingleChild(size_t index) {
    auto* sType = dyncast<ir::StructType const*>(_type);
    SC_ASSERT(sType && index < sType->members().size(), "Invalid index");
    if (_children.empty()) {
        _children.resize(sType->members().size());
    }
    auto& child = _children[index];
    if (!child) {
        child = std::make_unique<AccessTree>(sType->memberAt(index));
        child->_parent = this;
        child->_index = index;
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

void AccessTree::print(std::ostream& str) const {
    SC_UNIMPLEMENTED();
    //    for (int i = 0; i < level; ++i) {
    //        str << "    ";
    //    }
    //    if (index()) {
    //        str << *index() << ": ";
    //    }
    //    str << "[" << type()->name() << ", " << std::invoke(pt, payload())
    //        << "]\n";
    //    for (auto* c: children()) {
    //        if (c) {
    //            c->print(str, pt, level + 1);
    //        }
    //    }
}
