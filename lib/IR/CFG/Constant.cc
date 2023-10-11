#include "IR/CFG/Constant.h"

#include "IR/CFG.h"

using namespace scatha;
using namespace ir;

void Constant::writeValueTo(void* dest) const {
    visit(*this, [&](auto& derived) { derived.writeValueToImpl(dest); });
}
