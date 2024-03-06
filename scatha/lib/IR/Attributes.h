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

/// Function arguments passed by value but in memory
class SCATHA_API ByValAttribute: public Attribute {
public:
    explicit ByValAttribute(size_t size, size_t align);

    /// \Returns the size of the argument value
    size_t size() const { return _size; }

    /// \Returns the align of the argument value
    size_t align() const { return _align; }

private:
    uint16_t _size, _align;
};

} // namespace scatha::ir

#endif // SCATHA_IR_ATTRIBUTES_H_
