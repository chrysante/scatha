#include "IR/PointerInfo.h"

#include <utl/utility.hpp>

#include "IR/CFG/Value.h"

using namespace scatha;
using namespace ir;

PointerInfo::PointerInfo(PointerInfoDesc desc):
    _align(utl::narrow_cast<uint8_t>(desc.align)),
    _guaranteedNotNull(desc.guaranteedNotNull),
    _nonEscaping(desc.nonEscaping),
    _staticProv(desc.provenance.isStatic()),
    _prov(desc.provenance.value()),
    _validSize(desc.validSize.value_or(InvalidSize)),
    _staticProvOffset(desc.staticProvenanceOffset.value_or(InvalidSize)) {
    SC_ASSERT(provenance().value(), "Provenance cannot be null");
}

void PointerInfo::amend(PointerInfoDesc desc) {
    _align = std::max(utl::narrow_cast<uint8_t>(desc.align), _align);
    if (desc.validSize) {
        _validSize = *desc.validSize;
    }
    SC_ASSERT(desc.provenance == provenance(), "Provenance must be the same");
    if (desc.staticProvenanceOffset) {
        _staticProvOffset = *desc.staticProvenanceOffset;
    }
    _guaranteedNotNull |= desc.guaranteedNotNull;
    _nonEscaping |= desc.nonEscaping;
}

PointerInfoDesc PointerInfo::getDesc() const {
    return { .align = align(),
             .validSize = validSize(),
             .provenance = provenance(),
             .staticProvenanceOffset = staticProvenanceOffset(),
             .guaranteedNotNull = guaranteedNotNull(),
             .nonEscaping = isNonEscaping() };
}

void PointerInfo::setProvenance(PointerProvenance p,
                                std::optional<ssize_t> staticOffset) {
    _staticProv = p.isStatic();
    _prov = p.value();
    _staticProvOffset = staticOffset.value_or(InvalidSize);
}
