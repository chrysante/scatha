#include "Sema/Entity.h"

#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
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

void Entity::addAlternateName(std::string name) {
    _names.push_back(name);
    if (parent()) {
        parent()->addAlternateChildName(this, name);
    }
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
    /// We add all scopes to our list of child scopes
    if (auto* scope = dyncast<Scope*>(entity)) {
        bool const success = _children.insert(scope).second;
        SC_ASSERT(success, "Failed to add child");
    }
    /// We add the entity to our own symbol table
    /// We don't add anonymous entities because entities are keyed by their name
    /// We don't add functions because their names collide with their overload
    /// sets
    if (entity->isAnonymous() || isa<Function>(entity)) {
        return;
    }
    for (auto& name: entity->alternateNames()) {
        auto const [itr, success] = _entities.insert({ name, entity });
        SC_ASSERT(success, "Failed to add name");
    }
}

void Scope::addAlternateChildName(Entity* child, std::string name) {
    if (!_children.contains(child)) {
        return;
    }
    if (child->isAnonymous() || isa<Function>(child)) {
        return;
    }
    auto const [itr, success] = _entities.insert({ name, child });
    SC_ASSERT(success, "Failed to add new name");
}

AnonymousScope::AnonymousScope(ScopeKind scopeKind, Scope* parent):
    Scope(EntityType::AnonymousScope, scopeKind, std::string{}, parent) {}

GlobalScope::GlobalScope():
    Scope(EntityType::GlobalScope,
          ScopeKind::Global,
          "__GLOBAL__",

          nullptr) {}

/// # Types

size_t Type::size() const {
    return visit(*this, [](auto& derived) { return derived.sizeImpl(); });
}

size_t Type::align() const {
    return visit(*this, [](auto& derived) { return derived.alignImpl(); });
}

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

QualType::QualType(ObjectType* base, Reference ref):
    Type(EntityType::QualType,
         ScopeKind::Invalid,
         makeName(base, ref),
         base->parent()),
    _base(base),
    _ref(ref) {}

std::string QualType::makeName(ObjectType* base, Reference ref) {
    std::stringstream sstr;
    switch (ref) {
    case Reference::None:
        break;
    case Reference::Explicit:
        sstr << "&";
        break;
    case Reference::Implicit:
        sstr << "'";
        break;
    }
    sstr << base->name();
    return std::move(sstr).str();
}

size_t QualType::sizeImpl() const {
    if (isReference()) {
        return isa<ArrayType>(base()) ? 16 : 8;
    }
    return base()->size();
}

size_t QualType::alignImpl() const {
    if (isReference()) {
        return 8;
    }
    return base()->align();
}

/// # OverloadSet

static bool signatureMatch(std::span<QualType const* const> parameterTypes,
                           std::span<QualType const* const> argumentTypes) {
    if (parameterTypes.size() != argumentTypes.size()) {
        return false;
    }
    for (auto [param, arg]: ranges::views::zip(parameterTypes, argumentTypes)) {
        if (!param || !arg) {
            return false;
        }
        auto* paramArray = dyncast<ArrayType const*>(param->base());
        auto* argArray   = dyncast<ArrayType const*>(arg->base());
        if (paramArray && argArray &&
            (paramArray->count() == argArray->count() ||
             paramArray->isDynamic()))
        {
            /// Avoid next case, rewrite this if possible
        }
        else if (param->base() != arg->base()) {
            return false;
        }
        /// Rewrite this to `param->isReference() == arg->isExplicitReference()`
        if (param->isReference() && !arg->isExplicitReference()) {
            return false;
        }
        if (!param->isReference() && arg->isExplicitReference()) {
            return false;
        }
    }
    return true;
}

bool Function::isBuiltin() const {
    return isExtern() && slot() == svm::BuiltinFunctionSlot;
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
