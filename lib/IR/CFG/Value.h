#ifndef SCATHA_IR_CFG_VALUE_H_
#define SCATHA_IR_CFG_VALUE_H_

#include <string>
#include <string_view>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include "Common/Base.h"
#include "Common/Metadata.h"
#include "Common/Ranges.h"
#include "IR/Fwd.h"

namespace scatha::ir {

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA_API Value: public ObjectWithMetadata {
protected:
    explicit Value(NodeType nodeType,
                   Type const* type,
                   std::string name) noexcept:
        _nodeType(nodeType), _type(type), _name(std::move(name)) {}

public:
    /// Values are polymorphic objects and not copyable
    Value(Value const&) = delete;
    Value& operator=(Value const&) = delete;

    /// Calls `removeAllUses()`
    ~Value();

    /// The runtime type of this CFG node.
    NodeType nodeType() const { return _nodeType; }

    /// The type of this value.
    Type const* type() const { return _type; }

    /// The name of this value.
    std::string_view name() const { return _name; }

    /// Whether this value is named.
    bool hasName() const { return !_name.empty(); }

    /// Set the name of this value.
    void setName(std::string name);

    /// View of all users using this value.
    auto users() const { return _users | ranges::views::keys; }

    /// View of all users with use counts using this value.
    auto countedUsers() const { return _users | Opaque; }

    /// Number of users using this value. Multiple uses by the same user are
    /// counted as one.
    /// TODO: Rename to numUsers()
    size_t userCount() const { return _users.size(); }

    /// \Returns `true` if this value has no users
    bool unused() const { return userCount() == 0; }

    /// Clear all users from this values user list and update users
    void removeAllUses();

    /// Replace all uses of `this` with \p newValue
    void replaceAllUsesWith(Value* newValue);

    /// For complex initialization.
    void setType(Type const* type) { _type = type; }

private:
    friend class User;

    /// Register a user of this value. Won't affect \p user
    /// This function and `removeUserWeak()` are solely intended to be used be
    /// class `User`
    void addUserWeak(User* user);

    /// Unregister a user of this value. `this` will _not_ be cleared from the
    /// operand list of \p user
    void removeUserWeak(User* user);

    friend class BasicBlock;
    friend class Function;

    /// Unique the existing name of this value. This should be called when
    /// adding this value to a function.
    void uniqueExistingName(Function& func);

    /// Customization point for `UniquePtr`
    friend void ir::privateDelete(Value* value);

    /// Customization point for `ir::DynAllocator`
    friend void ir::privateDestroy(Value* value);

private:
    NodeType _nodeType;
    Type const* _type;
    std::string _name;
    utl::hashmap<User*, uint16_t> _users;
};

/// For `dyncast` compatibilty of the CFG
inline NodeType dyncast_get_type(std::derived_from<Value> auto const& value) {
    return value.nodeType();
}

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_VALUE_H_
