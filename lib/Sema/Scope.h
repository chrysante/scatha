// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SCOPE_H_
#define SCATHA_SEMA_SCOPE_H_

#include <memory>

#include <utl/hashmap.hpp>

#include <scatha/Sema/Entity.h>
#include <scatha/Sema/ScopeKind.h>

namespace scatha::sema {

namespace internal {
class ScopePrinter;
}

/// Represents a scope
class SCATHA_API Scope: public Entity {
public:
    /// The kind of this scope
    ScopeKind kind() const { return _kind; }

    // Until we have heterogenous lookup
    SymbolID findID(std::string_view name) const;

    bool isChildScope(SymbolID id) const { return _children.contains(id); }

    auto children() const;
    auto symbols() const;

protected:
    explicit Scope(EntityType entityType,
                   ScopeKind,
                   std::string name,
                   SymbolID symbolID,
                   Scope* parent);

private:
    friend class internal::ScopePrinter;
    friend class SymbolTable;
    void add(Entity* entity);

private:
    /// Scopes don't own their childscopes. These objects are owned by the
    /// symbol table.
    utl::hashmap<SymbolID, Scope*> _children;
    utl::hashmap<std::string, SymbolID> _symbols;
    ScopeKind _kind;
};

/// Represents an anonymous scope
class AnonymousScope: public Scope {
public:
    explicit AnonymousScope(SymbolID id, ScopeKind scopeKind, Scope* parent);
};

/// Represents the global scope
class GlobalScope: public Scope {
public:
    explicit GlobalScope(SymbolID id);
};

inline auto Scope::children() const {
    struct Iterator {
        Iterator begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        Iterator end() const {
            Iterator result = *this;
            result._itr     = _map.end();
            return result;
        }
        Scope const& operator*() const { return *_itr->second; }
        Iterator& operator++() {
            ++_itr;
            return *this;
        }
        bool operator==(Iterator const& rhs) const { return _itr == rhs._itr; }
        using Map = utl::hashmap<SymbolID, Scope*>;
        Map const& _map;
        Map::const_iterator _itr;
    };
    return Iterator{ _children, {} };
}

inline auto Scope::symbols() const {
    struct Iterator {
        Iterator begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        Iterator end() const {
            Iterator result = *this;
            result._itr     = _map.end();
            return result;
        }
        SymbolID operator*() const { return _itr->second; }
        Iterator& operator++() {
            ++_itr;
            return *this;
        }
        bool operator==(Iterator const& rhs) const { return _itr == rhs._itr; }
        using Map = utl::hashmap<std::string, SymbolID>;
        Map const& _map;
        Map::const_iterator _itr;
    };
    return Iterator{ _symbols, {} };
}

} // namespace scatha::sema

#endif // SCATHA_SEMA_SCOPE_H_
