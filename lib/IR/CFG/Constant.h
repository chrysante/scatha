#ifndef SCATHA_IR_CFG_CONSTANT_H_
#define SCATHA_IR_CFG_CONSTANT_H_

#include "Common/Base.h"
#include "IR/CFG/User.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a (global) constant value.
class SCATHA_API Constant: public User {
public:
    Constant(Type const* type, utl::vector<uint8_t> data, std::string name):
        User(NodeType::Constant, type, std::move(name)),
        _data(std::move(data)) {}

    /// To allow other classes to derive from this
    using User::User;

    /// The constant data
    std::span<uint8_t const> data() const { return _data; }

private:
    utl::vector<uint8_t> _data;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_CONSTANT_H_
