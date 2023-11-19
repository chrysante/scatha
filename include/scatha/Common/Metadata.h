#ifndef SCATHA_COMMON_METADATA_H_
#define SCATHA_COMMON_METADATA_H_

#include <any>

namespace scatha {

using Metadata = std::any;

/// Convenience base class to add metadata to objects
class ObjectWithMetadata {
public:
    ObjectWithMetadata() = default;

    explicit ObjectWithMetadata(Metadata metadata):
        _metadata(std::move(metadata)) {}

    /// \Returns the metadata associated with this value
    Metadata const& metadata() const { return _metadata; }

    /// Set the metadata associated with this value to \p metadata
    void setMetadata(Metadata metadata) { _metadata = std::move(metadata); }

private:
    Metadata _metadata;
};

} // namespace scatha

#endif // SCATHA_COMMON_METADATA_H_
