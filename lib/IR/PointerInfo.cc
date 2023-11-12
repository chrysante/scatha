#include "IR/PointerInfo.h"

#include "IR/CFG.h"
#include "IR/Type.h"

using namespace scatha;
using namespace ir;

PointerInfo ir::getPointerInfo(Value const& value) {
    SC_EXPECT(isa<PointerType>(value.type()));
    // clang-format off
    return SC_MATCH (value) {
        [](Value const&) { return PointerInfo{}; }
    }; // clang-format on
}
