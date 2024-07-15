#include "IR/Attributes.h"

#include <utl/utility.hpp>

using namespace scatha;
using namespace ir;

void ir::do_delete(ir::Attribute& attrib) {
    visit(attrib, [](auto& derived) { delete &derived; });
}

void ir::do_destroy(ir::Attribute& attrib) {
    visit(attrib, [](auto& derived) { std::destroy_at(&derived); });
}
