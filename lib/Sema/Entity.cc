#include "Sema/Entity.h"

#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/functional.hpp>
#include <utl/hash.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/NameMangling.h"

using namespace scatha;
using namespace sema;

void sema::privateDelete(sema::Entity* entity) {
    visit(*entity, [](auto& entity) { delete &entity; });
}

void sema::privateDestroy(sema::Entity* entity) {
    visit(*entity, [](auto& entity) { std::destroy_at(&entity); });
}

std::string const& Entity::mangledName() const {
    if (!_mangledName.empty()) {
        return _mangledName;
    }
    _mangledName = mangleName(this);
    return _mangledName;
}

EntityCategory Entity::category() const {
    return visit(*this, [](auto& derived) { return derived.categoryImpl(); });
}

void Entity::addAlternateName(std::string name) {
    _names.push_back(name);
    if (parent()) {
        parent()->addAlternateChildName(this, name);
    }
}

Object::Object(EntityType entityType,
               std::string name,
               Scope* parentScope,
               QualType type):
    Entity(entityType, std::move(name), parentScope), _type(type) {}

Object::~Object() = default;

void Object::setConstantValue(UniquePtr<Value> value) {
    if (value) {
        SC_ASSERT(type(), "Invalid");
        SC_ASSERT(!isa<ReferenceType>(*type()), "Invalid");
        SC_ASSERT(!type().isMutable(), "Invalid");
    }
    constVal = std::move(value);
}

Variable::Variable(std::string name, Scope* parentScope, QualType type):
    Object(EntityType::Variable, std::move(name), parentScope, type) {}

bool Variable::isLocal() const {
    return parent()->kind() == ScopeKind::Function ||
           parent()->kind() == ScopeKind::Anonymous;
}

Property::Property(PropertyKind kind, Scope* parentScope, QualType type):
    Object(EntityType::Property,
           std::string(toString(kind)),
           parentScope,
           type),
    _kind(kind) {}

Temporary::Temporary(size_t id, Scope* parentScope, QualType type):
    Object(EntityType::Temporary, std::string{}, parentScope, type), _id(id) {}

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
    SC_ASSERT((size() == InvalidSize) == (align() == InvalidSize),
              "Either both or neither must be invalid");
    return size() != InvalidSize;
}

bool ObjectType::isDefaultConstructible() const {
    return visit(*this,
                 [](auto& self) { return self.isDefaultConstructibleImpl(); });
}

bool ObjectType::hasTrivialLifetime() const {
    return visit(*this,
                 [](auto& self) { return self.hasTrivialLifetimeImpl(); });
}

VoidType::VoidType(Scope* parentScope):
    BuiltinType(EntityType::VoidType,
                "void",
                parentScope,
                InvalidSize,
                InvalidSize) {}

ArithmeticType::ArithmeticType(EntityType entityType,
                               std::string name,
                               size_t bitwidth,
                               Signedness signedness,
                               Scope* parentScope):
    BuiltinType(entityType,
                std::move(name),
                parentScope,
                utl::ceil_divide(bitwidth, 8),
                utl::ceil_divide(bitwidth, 8)),
    _signed(signedness),
    _bitwidth(utl::narrow_cast<uint16_t>(bitwidth)) {}

BoolType::BoolType(Scope* parentScope):
    ArithmeticType(EntityType::BoolType,
                   "bool",
                   1,
                   Signedness::Unsigned,
                   parentScope) {}

ByteType::ByteType(Scope* parentScope):
    ArithmeticType(EntityType::ByteType,
                   "byte",
                   8,
                   Signedness::Unsigned,
                   parentScope) {}

static std::string makeIntName(size_t bitwidth, Signedness signedness) {
    switch (signedness) {
    case Signedness::Signed:
        return utl::strcat("s", bitwidth);
    case Signedness::Unsigned:
        return utl::strcat("u", bitwidth);
    }
}

IntType::IntType(size_t bitwidth, Signedness signedness, Scope* parentScope):
    ArithmeticType(EntityType::IntType,
                   makeIntName(bitwidth, signedness),
                   bitwidth,
                   signedness,
                   parentScope) {}

static std::string makeFloatName(size_t bitwidth) {
    return utl::strcat("f", bitwidth);
}

FloatType::FloatType(size_t bitwidth, Scope* parentScope):
    ArithmeticType(EntityType::FloatType,
                   makeFloatName(bitwidth),
                   bitwidth,
                   Signedness::Signed,
                   parentScope) {
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "Invalid width");
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

static size_t ptrSize(QualType base) { return isa<ArrayType>(*base) ? 16 : 8; }

static size_t ptrAlign() { return 8; }

RefTypeBase::RefTypeBase(EntityType type, QualType base, std::string name):
    ObjectType(type,
               ScopeKind::Invalid,
               std::move(name),
               nullptr,
               ptrSize(base),
               ptrAlign()),
    _base(base) {}

static std::string makePtrName(QualType base, std::string_view op) {
    return utl::strcat(op, base.name());
}

PointerType::PointerType(QualType base):
    RefTypeBase(EntityType::PointerType, base, makePtrName(base, "*")) {}

ReferenceType::ReferenceType(QualType base):
    RefTypeBase(EntityType::ReferenceType, base, makePtrName(base, "&")) {}

bool Function::isBuiltin() const {
    return isForeign() && slot() == svm::BuiltinFunctionSlot;
}

std::pair<Function const*, bool> OverloadSet::add(Function* F) {
    auto itr = ranges::find_if(*this, [&](Function const* G) {
        return ranges::equal(F->argumentTypes(), G->argumentTypes());
    });
    if (itr == end()) {
        push_back(F);
        return { F, true };
    }
    return { *itr, false };
}
