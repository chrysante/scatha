#include "Common/Metadata.h"

using namespace scatha;

void ObjectWithMetadata::setMetadata(Metadata metadata) {
    if (_metadata) {
        *_metadata = std::move(metadata);
    }
    else {
        _metadata = std::make_unique<Metadata>(std::move(metadata));
    }
}
