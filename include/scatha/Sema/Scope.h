// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SCOPE_H_
#define SCATHA_SEMA_SCOPE_H_

#include <memory>

#include <utl/hashmap.hpp>

#include <scatha/Sema/EntityBase.h>
#include <scatha/Sema/ScopeKind.h>

namespace scatha::sema {

namespace internal {
class ScopePrinter;
}

class SCATHA(API) Scope: public EntityBase {
public:
    ScopeKind kind() const { return _kind; }
    explicit Scope(ScopeKind, SymbolID symbolID, Scope* parent);

protected:
public:
    explicit Scope(ScopeKind, std::string name, SymbolID symbolID, Scope* parent);

    // Until we have heterogenous lookup
    SymbolID findID(std::string_view name) const;

    bool isChildScope(SymbolID id) const { return _children.contains(id); }

    auto children() const;
    auto symbols() const;

private:
    friend class internal::ScopePrinter;
    friend class SymbolTable;
    void add(EntityBase& entity);
    void add(Scope& scopingEntity);

private:
    // Scopes don't own their childscopes. These objects are owned by the symbol
    // table.
    utl::hashmap<SymbolID, Scope*> _children;
    utl::hashmap<std::string, SymbolID> _symbols;
    ScopeKind _kind;
};

class GlobalScope: public Scope {
public:
    GlobalScope();
};

inline auto Scope::children() const {
    struct Iterator {
        auto begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        auto end() const {
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
        auto begin() const {
            Iterator result = *this;
            result._itr     = _map.begin();
            return result;
        }
        auto end() const {
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
