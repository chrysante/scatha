#include "IR/PointerInfo.h"

using namespace scatha;
using namespace ir;

void PointerInfo::setProvenance(Value* p, std::optional<size_t> staticOffset) {
    prov = p;
    if (staticOffset) {
        SC_ASSERT(*staticOffset < 1u << 15, "Too large to store");
        staticProvOffset = *staticOffset;
        hasStaticProvOffset = true;
    }
}
