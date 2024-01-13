#include "IRGen/MetaData.h"

using namespace scatha;
using namespace irgen;

void ObjectWithMetadata::setMetadata(Metadata metadata) {
    if (_metadata) {
        *_metadata = std::move(metadata);
    }
    else {
        _metadata = std::make_unique<Metadata>(std::move(metadata));
    }
}
