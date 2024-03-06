#ifndef SCATHA_IR_ATTRIBUTES_H_
#define SCATHA_IR_ATTRIBUTES_H_

#include "IR/Fwd.h"

namespace scatha::ir {

/// Base class of all attributes
class SCATHA_API Attribute {
public:
    /// \Returns the most derived type of this object
    AttributeType type() const { return _type; }

protected:
    explicit Attribute(AttributeType type): _type(type) {}

private:
    AttributeType _type;
};

/// For `dyncast` compatibilty
inline AttributeType dyncast_get_type(
    std::derived_from<Attribute> auto const& attrib) {
    return attrib.type();
}

/// Private base class of `ByValAttribute` and `ValRetAttribute`
class ByValAttribImpl {
public:
    /// \Returns the size of the passed value
    size_t size() const { return _size; }

    /// \Returns the align of the passed value
    size_t align() const { return _align; }

protected:
    explicit ByValAttribImpl(size_t size, size_t align);

private:
    uint16_t _size, _align;
};

/// Function argument passed by value but in memory
class SCATHA_API ByValAttribute: public Attribute, private ByValAttribImpl {
public:
    explicit ByValAttribute(size_t size, size_t align):
        Attribute(AttributeType::ByValAttribute),
        ByValAttribImpl(size, align) {}

    using ByValAttribImpl::align;
    using ByValAttribImpl::size;
};

/// Function return value passed in memory
class SCATHA_API ValRetAttribute: public Attribute, private ByValAttribImpl {
public:
    explicit ValRetAttribute(size_t size, size_t align):
        Attribute(AttributeType::ValRetAttribute),
        ByValAttribImpl(size, align) {}

    using ByValAttribImpl::align;
    using ByValAttribImpl::size;
};

} // namespace scatha::ir

#endif // SCATHA_IR_ATTRIBUTES_H_
