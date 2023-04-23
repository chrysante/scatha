// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_ENTITY_H_
#define SCATHA_SEMA_ENTITY_H_

#include <concepts>
#include <string>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/SymbolID.h>

namespace scatha::sema {

class Scope;

/// Base class for all semantic entities in the language.
class SCATHA_API Entity {
public:
    struct MapHash: std::hash<SymbolID> {
        struct is_transparent;
        using std::hash<SymbolID>::operator();
        size_t operator()(Entity const& e) const {
            return std::hash<SymbolID>{}(e.symbolID());
        }
    };

    struct MapEqual {
        struct is_transparent;
        bool operator()(Entity const& a, Entity const& b) const {
            return a.symbolID() == b.symbolID();
        }
        bool operator()(Entity const& a, SymbolID b) const {
            return a.symbolID() == b;
        }
        bool operator()(SymbolID a, Entity const& b) const {
            return a == b.symbolID();
        }
    };

public:
    /// The name of this entity
    std::string_view name() const { return _name; }

    /// The SymbolID of this entity
    SymbolID symbolID() const { return _symbolID; }

    /// `true` if this entity is unnamed
    bool isAnonymous() const { return _name.empty(); }

    /// The parent scope of this entity
    Scope* parent() { return _parent; }

    /// \overload
    Scope const* parent() const { return _parent; }

    /// The runtime type of this entity class
    EntityType entityType() const { return _entityType; }

protected:
    explicit Entity(EntityType entityType,
                    std::string name,
                    SymbolID symbolID,
                    Scope* parent):
        _entityType(entityType),
        _name(name),
        _symbolID(symbolID),
        _parent(parent) {}

private:
    EntityType _entityType;
    std::string _name;
    SymbolID _symbolID;
    Scope* _parent = nullptr;
};

EntityType dyncast_get_type(std::derived_from<Entity> auto const& entity) {
    return entity.entityType();
}

} // namespace scatha::sema

#endif // SCATHA_SEMA_ENTITY_H_
