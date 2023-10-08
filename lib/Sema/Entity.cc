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
               Type const* type,
               Mutability mut,
               ast::ASTNode* astNode):
    Entity(entityType, std::move(name), parentScope, astNode),
    _type(type),
    _mut(mut) {}

Object::~Object() = default;

QualType Object::getQualType() const {
    if (auto* ref = dyncast_or_null<ReferenceType const*>(type())) {
        return ref->base();
    }
    return QualType(cast_or_null<ObjectType const*>(type()), mutability());
}

void Object::setConstantValue(UniquePtr<Value> value) {
    if (value) {
        SC_ASSERT(type(), "Object must have a type to have a constant value");
        SC_ASSERT(isConst(), "Only const objects have constant values");
    }
    constVal = std::move(value);
}

Variable::Variable(std::string name,
                   Scope* parentScope,
                   ast::ASTNode* astNode,
                   Type const* type,
                   Mutability mut):
    Object(EntityType::Variable,
           std::move(name),
           parentScope,
           type,
           mut,
           astNode) {}

bool Variable::isLocal() const {
    return parent()->kind() == ScopeKind::Function;
}

Property::Property(PropertyKind kind,
                   Scope* parentScope,
                   Type const* type,
                   Mutability mut,
                   ValueCategory valueCat):
    Object(EntityType::Property,
           std::string(toString(kind)),
           parentScope,
           type,
           mut),
    _kind(kind),
    _valueCat(valueCat) {}

Temporary::Temporary(size_t id, Scope* parentScope, QualType type):
    Object(EntityType::Temporary,
           std::string{},
           parentScope,
           type.get(),
           type.mutability()),
    _id(id) {}

Scope::Scope(EntityType entityType,
             ScopeKind kind,
             std::string name,
             Scope* parent,
             ast::ASTNode* astNode):
    Entity(entityType, std::move(name), parent, astNode), _kind(kind) {}

Entity const* Scope::findEntity(std::string_view name) const {
    auto const itr = _entities.find(name);
    return itr == _entities.end() ? nullptr : itr->second;
}

Property const* Scope::findProperty(PropertyKind kind) const {
    auto const itr = _properties.find(kind);
    return itr == _properties.end() ? nullptr : itr->second;
}

void Scope::add(Entity* entity) {
    /// Each scope that we add we add to to our list of child scopes
    if (auto* scope = dyncast<Scope*>(entity)) {
        bool const success = _children.insert(scope).second;
        SC_ASSERT(success, "Failed to add child");
    }
    if (auto* prop = dyncast<Property*>(entity)) {
        SC_ASSERT(!_properties.contains(prop->kind()),
                  "Property already exists in this scope");
        _properties.insert({ prop->kind(), prop });
    }
    /// We add the entity to our own symbol table
    /// We don't add anonymous entities because entities are keyed by their name
    /// We don't add functions because their names collide with their overload
    /// sets
    if (entity->isAnonymous() || isa<Function>(entity)) {
        return;
    }
    for (auto& name: entity->alternateNames()) {
        SC_ASSERT(!_entities.contains(name),
                  "Name already exists in this scope");
        _entities.insert({ name, entity });
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

bool Type::isComplete() const { return size() != InvalidSize; }

bool Type::isDefaultConstructible() const {
    return visit(*this,
                 [](auto& self) { return self.isDefaultConstructibleImpl(); });
}

bool Type::hasTrivialLifetime() const {
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

static std::string makeIndirectName(std::string_view indirection,
                                    QualType base) {
    return utl::strcat(indirection, base.qualName());
}

PointerType::PointerType(QualType base):
    ObjectType(EntityType::PointerType,
               ScopeKind::Invalid,
               makeIndirectName("*", base),
               const_cast<Scope*>(base->parent()),
               ptrSize(base),
               ptrAlign()),
    PtrRefTypeBase(base) {}

ReferenceType::ReferenceType(QualType base):
    Type(EntityType::ReferenceType,
         ScopeKind::Invalid,
         makeIndirectName("&", base),
         const_cast<Scope*>(base->parent())),
    PtrRefTypeBase(base) {}

void Function::setDeducedReturnType(Type const* type) {
    SC_ASSERT(!returnType(), "Already set");
    _sig._returnType = type;
}

bool Function::isBuiltin() const {
    return isForeign() && slot() == svm::BuiltinFunctionSlot;
}

Function* OverloadSet::add(Function* F) {
    auto itr = ranges::find_if(*this, [&](Function const* G) {
        return ranges::equal(F->argumentTypes(), G->argumentTypes());
    });
    if (itr == end()) {
        push_back(F);
        return nullptr;
    }
    return *itr;
}

Function const* OverloadSet::find(std::span<Type const* const> types) const {
    auto itr = ranges::find_if(*this, [&](auto* function) {
        return ranges::equal(function->argumentTypes(), types);
    });
    return itr != end() ? *itr : nullptr;
}
