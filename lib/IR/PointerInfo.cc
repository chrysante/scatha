#include "IR/PointerInfo.h"

using namespace scatha;
using namespace ir;

PointerInfo::PointerInfo(PointerInfoDesc desc):
    _align(desc.align),
    _hasRange(desc.validSize),
    _range(desc.validSize.value_or(0)) {
    setProvenance(desc.provenance, desc.staticProvenanceOffset);
}

void PointerInfo::setProvenance(Value* p, std::optional<size_t> staticOffset) {
    prov = p;
    if (staticOffset) {
        SC_ASSERT(*staticOffset < 1u << 15, "Too large to store");
        staticProvOffset = *staticOffset;
        hasStaticProvOffset = true;
    }
}
