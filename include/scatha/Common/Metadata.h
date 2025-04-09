#ifndef SCATHA_COMMON_METADATA_H_
#define SCATHA_COMMON_METADATA_H_

#include <memory>
#include <ostream>

namespace scatha {

/// Metadata interface
class Metadata {
public:
    virtual ~Metadata() = default;

    /// Print this metadata to \p os
    void prettyPrint(std::ostream& os) const { doPrettyPrint(os); }

protected:
    Metadata() = default;
    Metadata(Metadata const&) = default;

private:
    /// \Returns a clone of \p md
    /// Can be called with null pointers
    friend std::unique_ptr<Metadata> clone(Metadata const* md) {
        return md ? md->doClone() : nullptr;
    }

    virtual std::unique_ptr<Metadata> doClone() const = 0;
    virtual void doPrettyPrint(std::ostream& os) const = 0;
};

/// Convenience base class to add metadata to objects
class ObjectWithMetadata {
public:
    ObjectWithMetadata() = default;

    explicit ObjectWithMetadata(std::unique_ptr<Metadata> metadata):
        _metadata(std::move(metadata)) {}

    ObjectWithMetadata(ObjectWithMetadata const& rhs):
        ObjectWithMetadata(rhs.cloneMetadata()) {}

    ObjectWithMetadata& operator=(ObjectWithMetadata const& rhs) {
        setMetadata(rhs.cloneMetadata());
        return *this;
    }

    ObjectWithMetadata(ObjectWithMetadata&&) = default;

    ObjectWithMetadata& operator=(ObjectWithMetadata&&) = default;

    /// \Returns the associated metadata
    Metadata const* metadata() const { return _metadata.get(); }

    /// \Returns the metadata as a derived type
    template <std::derived_from<Metadata> MD>
    MD const* metadataAs() const {
        return dynamic_cast<MD const*>(metadata());
    }

    /// \Returns a clone of this objects metadata
    std::unique_ptr<Metadata> cloneMetadata() const {
        return clone(metadata());
    }

    /// Set the metadata associated with this value to \p md
    void setMetadata(std::unique_ptr<Metadata> md) {
        _metadata = std::move(md);
    }

private:
    std::unique_ptr<Metadata> _metadata;
};

} // namespace scatha

#endif // SCATHA_COMMON_METADATA_H_
