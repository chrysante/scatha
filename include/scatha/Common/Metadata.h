#ifndef SCATHA_COMMON_METADATA_H_
#define SCATHA_COMMON_METADATA_H_

#include <any>
#include <memory>

namespace scatha {

using Metadata = std::any;

/// Convenience base class to add metadata to objects
class ObjectWithMetadata {
public:
    ObjectWithMetadata() = default;

    explicit ObjectWithMetadata(Metadata metadata):
        _metadata(metadata.has_value() ?
                      std::make_unique<Metadata>(std::move(metadata)) :
                      nullptr) {}

    ObjectWithMetadata(ObjectWithMetadata const& rhs):
        ObjectWithMetadata(rhs.metadata()) {}

    ObjectWithMetadata& operator=(ObjectWithMetadata const& rhs) {
        setMetadata(rhs.metadata());
        return *this;
    }

    ObjectWithMetadata(ObjectWithMetadata&&) = default;

    ObjectWithMetadata& operator=(ObjectWithMetadata&&) = default;

    /// \Returns the metadata associated with this value
    Metadata metadata() const { return _metadata ? *_metadata : Metadata{}; }

    /// Set the metadata associated with this value to \p metadata
    void setMetadata(Metadata metadata);

private:
    std::unique_ptr<Metadata> _metadata;
};

} // namespace scatha

#endif // SCATHA_COMMON_METADATA_H_
