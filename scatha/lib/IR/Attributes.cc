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
