#include "IR/Attributes.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace ir;

void ir::privateDelete(ir::Attribute* attrib) {
    visit(*attrib, [](auto& derived) { delete &derived; });
}

void ir::privateDestroy(ir::Attribute* attrib) {
    visit(*attrib, [](auto& derived) { std::destroy_at(&derived); });
}

ByValAttribImpl::ByValAttribImpl(size_t size, size_t align):
    _size(utl::narrow_cast<uint16_t>(size)),
    _align(utl::narrow_cast<uint16_t>(align)) {}
