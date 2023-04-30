#include "Sema/Entity.h"

#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/utility.hpp>

#include "Sema/NameMangling.h"

using namespace scatha;
using namespace sema;

std::string const& Entity::mangledName() const {
    if (!_mangledName.empty()) {
        return _mangledName;
    }
    _mangledName = mangleName(this);
    return _mangledName;
}

/// # Variable

sema::Variable::Variable(std::string name,
                         Scope* parentScope,
                         QualType const* type):
    Entity(EntityType::Variable, std::move(name), parentScope), _type(type) {}

bool sema::Variable::isLocal() const {
    return parent()->kind() == ScopeKind::Function ||
           parent()->kind() == ScopeKind::Anonymous;
}

/// # Scopes

Scope::Scope(EntityType entityType,
             ScopeKind kind,
             std::string name,
             Scope* parent):
    Entity(entityType, std::move(name), parent), _kind(kind) {}

Entity const* Scope::findEntity(std::string_view name) const {
    auto const itr = _entities.find(name);
    return itr == _entities.end() ? nullptr : itr->second;
}

void Scope::add(Entity* entity) {
    auto impl = [this](Entity* entity) {
        auto const [itr, success] =
            _entities.insert({ std::string(entity->name()), entity });
        SC_ASSERT(success, "");
    };
    if (auto* scope = dyncast<Scope*>(entity)) {
        if (!scope->isAnonymous() &&
            /// Can't add functions here because of name collisions due to
            /// overloading.
            scope->kind() != ScopeKind::Function)
        {
            impl(scope);
        }
        auto const [itr, success] = _children.insert(scope);
        SC_ASSERT(success, "");
    }
    else {
        impl(entity);
    }
}

AnonymousScope::AnonymousScope(ScopeKind scopeKind, Scope* parent):
    Scope(EntityType::AnonymousScope, scopeKind, std::string{}, parent) {}

GlobalScope::GlobalScope():
    Scope(EntityType::GlobalScope,
          ScopeKind::Global,
          "__GLOBAL__",

          nullptr) {}

/// # Types

bool Type::isComplete() const {
    SC_ASSERT((size() == invalidSize) == (align() == invalidSize),
              "Either both or neither must be invalid");
    return size() != invalidSize;
}

std::string ArrayType::makeName(ObjectType const* elemType, size_t count) {
    std::stringstream sstr;
    sstr << "[" << elemType->name();
    if (count != DynamicCount) {
        sstr << "," << count;
    }
    sstr << "]";
    return std::move(sstr).str();
}

std::string QualType::makeName(ObjectType* base, TypeQualifiers qualifiers) {
    using enum TypeQualifiers;
    std::stringstream sstr;
    if (test(qualifiers & (ExplicitReference | ImplicitReference))) {
        sstr << "&";
    }
    if (test(qualifiers & Mutable)) {
        sstr << "mut ";
    }
    sstr << base->name();
    return std::move(sstr).str();
}

/// # OverloadSet

static bool signatureMatch(std::span<QualType const* const> parameterTypes,
                           std::span<QualType const* const> argumentTypes) {
    if (parameterTypes.size() != argumentTypes.size()) {
        return false;
    }
    for (auto [param, arg]: ranges::views::zip(parameterTypes, argumentTypes)) {
        if (param->base() != arg->base()) {
            return false;
        }
        SC_ASSERT(!param->has(TypeQualifiers::ExplicitReference), "");
        if (param->has(TypeQualifiers::ImplicitReference) &&
            !arg->has(TypeQualifiers::ExplicitReference))
        {
            return false;
        }
        if (!param->has(TypeQualifiers::ImplicitReference) &&
            arg->has(TypeQualifiers::ExplicitReference))
        {
            return false;
        }
    }
    return true;
}

Function const* OverloadSet::find(
    std::span<QualType const* const> argumentTypes) const {
    auto matches = functions | ranges::views::filter([&](Function* F) {
                       return signatureMatch(F->signature().argumentTypes(),
                                             argumentTypes);
                   }) |
                   ranges::to<utl::small_vector<Function*>>;
    if (matches.size() == 1) {
        return matches.front();
    }
    return nullptr;
}

std::pair<Function const*, bool> OverloadSet::add(Function* F) {
    auto itr = ranges::find_if(functions, [&](Function const* G) {
        return argumentsEqual(F->signature(), G->signature());
    });
    if (itr == ranges::end(functions)) {
        functions.push_back(F);
        return { F, true };
    }
    return { *itr, false };
}
