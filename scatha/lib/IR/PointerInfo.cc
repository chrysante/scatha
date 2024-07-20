#include "IR/PointerInfo.h"

#include <utl/utility.hpp>

#include "IR/CFG/Value.h"

using namespace scatha;
using namespace ir;

PointerInfo::PointerInfo(PointerInfoDesc desc):
    _align(utl::narrow_cast<uint8_t>(desc.align)),
    _guaranteedNotNull(desc.guaranteedNotNull),
    _prov(desc.provenance),
    _validSize(desc.validSize.value_or(InvalidSize)),
    _staticProvOffset(desc.staticProvenanceOffset.value_or(InvalidSize)) {}

void PointerInfo::setProvenance(Value* p, std::optional<ssize_t> staticOffset) {
    _prov = p;
    _staticProvOffset = staticOffset.value_or(InvalidSize);
}
