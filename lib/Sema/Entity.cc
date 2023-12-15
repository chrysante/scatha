#include "Sema/Entity.h"

#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/functional.hpp>
#include <utl/hash.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
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

std::optional<AccessSpecifier> Entity::accessSpec() const {
    if (auto* decl = dyncast<ast::Declaration const*>(astNode())) {
        return decl->accessSpec();
    }
    return std::nullopt;
}

void Entity::addAlternateName(std::string name) {
    _names.push_back(name);
    if (parent()) {
        parent()->addAlternateChildName(this, name);
    }
}

void Entity::addParent(Scope* parent) {
    if (ranges::contains(_parents, parent)) {
        return;
    }
    _parents.push_back(parent);
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
    return sema::getQualType(type(), mutability());
}

void Object::setConstantValue(UniquePtr<Value> value) {
    if (value) {
        SC_ASSERT(type(), "Object must have a type to have a constant value");
        SC_ASSERT(isConst(), "Only const objects have constant values");
    }
    constVal = std::move(value);
}

ValueCategory VarBase::valueCategory() const {
    return visit(*this, [](auto& derived) { return derived.valueCatImpl(); });
}

Variable::Variable(std::string name,
                   Scope* parentScope,
                   ast::ASTNode* astNode,
                   Type const* type,
                   Mutability mut):
    VarBase(EntityType::Variable,
            std::move(name),
            parentScope,
            type,
            mut,
            astNode) {}

Property::Property(PropertyKind kind,
                   Scope* parentScope,
                   Type const* type,
                   Mutability mut,
                   ValueCategory valueCat):
    VarBase(EntityType::Property,
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

std::span<Entity const* const> Scope::findEntities(
    std::string_view name) const {
    auto const itr = _names.find(name);
    if (itr != _names.end()) {
        return itr->second;
    }
    return {};
}

Property const* Scope::findProperty(PropertyKind kind) const {
    auto const itr = _properties.find(kind);
    return itr == _properties.end() ? nullptr : itr->second;
}

void Scope::addChild(Entity* entity) {
    entity->addParent(this);
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
    if (entity->isAnonymous()) {
        return;
    }
    for (auto& name: entity->alternateNames()) {
        _names[name].push_back(entity);
    }
}

void Scope::addAlternateChildName(Entity* child, std::string name) {
    if (child->isAnonymous() || !_children.contains(child)) {
        return;
    }
    auto& list = _names[name];
    SC_ASSERT(list.empty(), "Failed to add new name");
    list.push_back(child);
}

AnonymousScope::AnonymousScope(ScopeKind scopeKind, Scope* parent):
    Scope(EntityType::AnonymousScope, scopeKind, std::string{}, parent) {}

GlobalScope::GlobalScope():
    Scope(EntityType::GlobalScope, ScopeKind::Global, "__GLOBAL__", nullptr) {}

FileScope::FileScope(std::string filename, Scope* parent):
    Scope(EntityType::FileScope,
          ScopeKind::Global,
          std::move(filename),
          parent) {}

LibraryScope::LibraryScope(std::string name, Scope* parent):
    Scope(EntityType::LibraryScope,
          ScopeKind::Global,
          std::move(name),
          parent) {}

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

NullPtrType::NullPtrType(Scope* parent):
    BuiltinType(EntityType::NullPtrType, "<null-type>", parent, 1, 1) {}

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

PointerType::PointerType(EntityType entityType,
                         QualType base,
                         std::string name):
    ObjectType(entityType,
               ScopeKind::Invalid,
               std::move(name),
               const_cast<Scope*>(base->parent()),
               ptrSize(base),
               ptrAlign()),
    PtrRefTypeBase(base) {}

RawPtrType::RawPtrType(QualType base):
    PointerType(EntityType::RawPtrType, base, makeIndirectName("*", base)) {}

UniquePtrType::UniquePtrType(QualType base):
    PointerType(EntityType::UniquePtrType,
                base,
                makeIndirectName("*unique ", base)) {}

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

Function const* OverloadSet::find(std::span<Type const* const> types) const {
    return find(std::span<Function const* const>(*this), types);
}

Function const* OverloadSet::find(std::span<Function const* const> set,
                                  std::span<Type const* const> types) {
    auto itr = ranges::find_if(set, [&](auto* function) {
        return ranges::equal(function->argumentTypes(), types);
    });
    return itr != set.end() ? *itr : nullptr;
}
