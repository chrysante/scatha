#ifndef SCATHA_IR_CFG_VALUE_H_
#define SCATHA_IR_CFG_VALUE_H_

#include <memory>
#include <string>
#include <string_view>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Metadata.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>

namespace scatha::ir {

/// Represents a value in the program.
/// Every value has a type. Types are not values.
class SCATHA_API Value: public ObjectWithMetadata {
public:
    /// Values are polymorphic objects and not copyable
    Value(Value const&) = delete;
    Value& operator=(Value const&) = delete;

    /// Calls `removeAllUses()` and `clearAllReferences()`
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
    auto const& countedUsers() const { return _users; }

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

    /// Sets all `ValueRef`s to this value to null
    void clearAllReferences();

    /// For complex initialization.
    void setType(Type const* type) { _type = type; }

    /// Pointer info associated with this value
    PointerInfo const* pointerInfo() const { return ptrInfo.get(); }

    /// Allocates a pointer info object for this value.
    void setPointerInfo(PointerInfoDesc const& desc);

    /// Allocates a pointer info object for this value or amends the existing
    /// pointer info.
    void amendPointerInfo(PointerInfoDesc const& desc);

    /// \Returns a view over the attributes of this value (`[Attribute const*]`)
    auto attributes() const {
        return _attribs.values() | ranges::views::values | ToConstAddress;
    }

    /// Constructs the attribute type \p Attrib from \p args... and adds it to
    /// this value
    template <std::derived_from<Attribute> Attrib, typename... Args>
        requires std::constructible_from<Attrib, Args...>
    Attribute const* addAttribute(Args&&... args) {
        return addAttribute(allocate<Attrib>(std::forward<Args>(args)...));
    }

    /// \overload
    Attribute const* addAttribute(UniquePtr<Attribute> attrib);

    /// Removes the attribute of type \p attribType
    void removeAttribute(AttributeType attribType);

    ///
    Attribute const* get(AttributeType attrib) const;

    ///
    template <std::derived_from<Attribute> Attrib>
    Attrib const* get() const {
        return cast<Attrib const*>(get(csp::impl::TypeToID<Attrib>));
    }

protected:
    explicit Value(NodeType nodeType, Type const* type,
                   std::string name) noexcept;

private:
    friend class User;
    friend class ValueRef;

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

    friend NodeType get_rtti(Value const& value) { return value.nodeType(); }

    NodeType _nodeType;
    uint16_t _ptrInfoArrayCount = 0;
    Type const* _type;
    std::string _name;
    utl::hashmap<User*, uint16_t> _users;
    utl::hashset<ValueRef*> _references;
    utl::hashmap<AttributeType, UniquePtr<Attribute>> _attribs;
    std::unique_ptr<PointerInfo> ptrInfo;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_VALUE_H_
