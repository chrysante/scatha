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
    /// \Returns the type of the passed value
    Type const* type() const { return _type; }

protected:
    explicit ByValAttribImpl(Type const* type): _type(type) {}

private:
    Type const* _type;
};

/// Function argument passed by value but in memory
class SCATHA_API ByValAttribute: public Attribute, private ByValAttribImpl {
public:
    explicit ByValAttribute(Type const* type):
        Attribute(AttributeType::ByValAttribute), ByValAttribImpl(type) {}

    using ByValAttribImpl::type;
};

/// Function return value passed in memory
class SCATHA_API ValRetAttribute: public Attribute, private ByValAttribImpl {
public:
    explicit ValRetAttribute(Type const* type):
        Attribute(AttributeType::ValRetAttribute), ByValAttribImpl(type) {}

    using ByValAttribImpl::type;
};

} // namespace scatha::ir

#endif // SCATHA_IR_ATTRIBUTES_H_
