#include "Sema/Entity.h"

#include <memory>
#include <sstream>

#include <range/v3/algorithm.hpp>
#include <range/v3/numeric.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/functional.hpp>
#include <utl/hash.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/NameMangling.h"
#include "Sema/VTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

void sema::do_delete(sema::Entity& entity) {
    visit(entity, [](auto& entity) { delete &entity; });
}

void sema::do_destroy(sema::Entity& entity) {
    visit(entity, [](auto& entity) { std::destroy_at(&entity); });
}

EntityCategory Entity::category() const {
    return visit(*this, [](auto& derived) { return derived.categoryImpl(); });
}

Entity::Entity(EntityType entityType, std::string name, Scope* parent,
               ast::ASTNode* astNode):
    _entityType(entityType),
    _parent(parent),
    _name(std::move(name)),
    _astNode(astNode) {}

void Entity::setParent(Scope* parent) { _parent = parent; }

void Entity::addAlias(Alias* alias) {
    SC_EXPECT(!ranges::contains(_aliases, alias));
    _aliases.push_back(alias);
}

Type const* sema::getEntityType(Entity const& entity) {
    return visit(entity, []<typename E>(E const& entity) -> Type const* {
        if constexpr (requires { entity.type(); }) {
            return entity.type();
        }
        return nullptr;
    });
}

Object::Object(EntityType entityType, std::string name, Scope* parentScope,
               Type const* type, Mutability mut, PointerBindMode bindMode,
               ast::ASTNode* astNode):
    Entity(entityType, std::move(name), parentScope, astNode),
    _type(type),
    _mut(mut),
    _bindMode(bindMode) {}

Object::~Object() = default;

QualType Object::getQualType() const {
    return sema::getQualType(type(), mutability(), bindMode());
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

Variable::Variable(std::string name, Scope* parentScope, ast::ASTNode* astNode,
                   AccessControl accessControl, Type const* type,
                   Mutability mut):
    VarBase(EntityType::Variable, std::move(name), parentScope, type, mut,
            PointerBindMode::Static, astNode) {
    setAccessControl(accessControl);
}

bool Variable::isStatic() const {
    return isa<FileScope>(parent()) || isa<GlobalScope>(parent()) ||
           isa<Library>(parent());
}

BaseClassObject::BaseClassObject(std::string name, Scope* parentScope,
                                 ast::ASTNode* astNode,
                                 AccessControl accessControl,
                                 RecordType const* type):
    Object(EntityType::BaseClassObject, std::move(name), parentScope, type,
           Mutability::Mutable, PointerBindMode::Static, astNode) {
    setAccessControl(accessControl);
}

RecordType const* BaseClassObject::type() const {
    return dyncast<RecordType const*>(Object::type());
}

Property::Property(PropertyKind kind, Scope* parentScope, Type const* type,
                   Mutability mut, PointerBindMode bindMode,
                   ValueCategory valueCat, AccessControl accessControl,
                   ast::ASTNode* astNode):
    VarBase(EntityType::Property, std::string(toString(kind)), parentScope,
            type, mut, bindMode, astNode),
    _kind(kind),
    _valueCat(valueCat) {
    setAccessControl(accessControl);
}

Temporary::Temporary(size_t id, Scope* parentScope, QualType type,
                     ast::ASTNode* node):
    Object(EntityType::Temporary, std::string{}, parentScope, type.get(),
           type.mutability(), type.bindMode(), node),
    _id(id) {}

Scope::Scope(EntityType entityType, ScopeKind kind, std::string name,
             Scope* parent, ast::ASTNode* astNode):
    Entity(entityType, std::move(name), parent, astNode), _kind(kind) {}

utl::tiny_ptr_vector<Entity*> Scope::findEntities(std::string_view name,
                                                  bool findHidden) {
    return findEntitiesImpl<Entity>(this, name, findHidden);
}

utl::tiny_ptr_vector<Entity const*> Scope::findEntities(std::string_view name,
                                                        bool findHidden) const {
    return findEntitiesImpl<Entity const>(this, name, findHidden);
}

template <typename E, typename S>
utl::tiny_ptr_vector<E*> Scope::findEntitiesImpl(S* self, std::string_view name,
                                                 bool findHidden) {
    auto itr = self->_names.find(name);
    if (itr == self->_names.end()) {
        return {};
    }
    auto const& entities = itr->second;
    if (findHidden) {
        return utl::tiny_ptr_vector<E*>(entities.begin(), entities.end());
    }
    auto visible = entities | filter(&Entity::isVisible);
    return utl::tiny_ptr_vector<E*>(visible.begin(), visible.end());
}

Property const* Scope::findProperty(PropertyKind kind) const {
    auto const itr = _properties.find(kind);
    return itr == _properties.end() ? nullptr : itr->second;
}

template <typename F>
static utl::small_vector<F*> findFunctionsImpl(auto& scope,
                                               std::string_view name) {
    auto entities = scope.findEntities(name);
    return entities | transform([](auto* entity) {
        return cast<F*>(stripAlias(entity));
    }) | ToSmallVector<>;
}

utl::small_vector<Function*> Scope::findFunctions(std::string_view name) {
    return findFunctionsImpl<Function>(*this, name);
}

utl::small_vector<Function const*> Scope::findFunctions(
    std::string_view name) const {
    return findFunctionsImpl<Function const>(*this, name);
}

void Scope::addChild(Entity* entity) {
    SC_ASSERT(entity->parent() == nullptr || entity->parent() == this,
              "entity already has a parent");
    entity->setParent(this);
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
    if (!entity->isAnonymous()) {
        auto& entities = _names[entity->name()];
        SC_ASSERT(!ranges::contains(entities, entity),
                  "entity already is our child");
        entities.push_back(entity);
    }
}

void Scope::removeChild(Entity* entity) {
    /// This function is basically the reverse of `addChild()`
    if (auto* scope = dyncast<Scope const*>(entity)) {
        _children.erase(scope);
    }
    if (auto* prop = dyncast<Property const*>(entity)) {
        _properties.erase(prop->kind());
    }
    if (!entity->isAnonymous()) {
        auto& entities = _names[entity->name()];
        auto itr = ranges::find(entities, entity);
        SC_ASSERT(itr != entities.end(), "entity is not a child");
        entities.erase(itr);
    }
    entity->setParent(nullptr);
}

AnonymousScope::AnonymousScope(ScopeKind scopeKind, Scope* parent):
    Scope(EntityType::AnonymousScope, scopeKind, std::string{}, parent) {}

GlobalScope::GlobalScope():
    Scope(EntityType::GlobalScope, ScopeKind::Global, {}, nullptr) {}

FileScope::FileScope(size_t index, std::string filename, Scope* parent):
    Scope(EntityType::FileScope, ScopeKind::Global, std::move(filename),
          parent),
    _index(index) {}

Library::Library(EntityType entityType, std::string name, Scope* parent):
    Scope(entityType, ScopeKind::Global, std::move(name), parent) {}

NativeLibrary::NativeLibrary(std::string name, std::filesystem::path path,
                             Scope* parent):
    Library(EntityType::NativeLibrary, std::move(name), parent),
    _path(std::move(path)) {}

ForeignLibrary::ForeignLibrary(std::string name, std::filesystem::path file,
                               Scope* parent):
    Library(EntityType::ForeignLibrary, std::move(name), parent),
    _file(std::move(file)) {}

/// # Types

Type::Type(EntityType entityType, ScopeKind scopeKind, std::string name,
           Scope* parent, ast::ASTNode* astNode, AccessControl accessControl):
    Scope(entityType, scopeKind, std::move(name), parent, astNode) {
    setAccessControl(accessControl);
}

size_t Type::size() const {
    return visit(*this, [](auto& derived) { return derived.sizeImpl(); });
}

size_t Type::align() const {
    return visit(*this, [](auto& derived) { return derived.alignImpl(); });
}

bool Type::isComplete() const { return size() != InvalidSize; }

bool Type::isCompleteOrVoid() const {
    return isComplete() || isa<VoidType>(this);
}

bool Type::hasTrivialLifetime() const {
    if (isa<ProtocolType>(this)) {
        return false;
    }
    if (auto* objType = dyncast<ObjectType const*>(this)) {
        return objType->lifetimeMetadata().trivialLifetime();
    }
    return true;
}

FunctionType::FunctionType(std::span<Type const* const> argumentTypes,
                           Type const* returnType):
    FunctionType(argumentTypes | ToSmallVector<>, returnType) {}

static std::string getTypename(Type const* type) {
    if (type) {
        return std::string(type->name());
    }
    return "NULL";
}

static std::string makeFunctionTypeName(
    std::span<Type const* const> argumentTypes, Type const* returnType) {
    std::stringstream sstr;
    sstr << "(";
    for (bool first = true; auto* arg: argumentTypes) {
        if (!first) sstr << ", ";
        first = false;
        sstr << getTypename(arg);
    }
    sstr << ") -> " << getTypename(returnType);
    return std::move(sstr).str();
}

static AccessControl computeFnTypeAccCtrl(
    std::span<Type const* const> argumentTypes, Type const* returnType) {
    auto getAccCtrl = [](Type const* type) {
        return type ? type->accessControl() : AccessControl::Public;
    };
    return ranges::accumulate(argumentTypes, getAccCtrl(returnType),
                              ranges::max, getAccCtrl);
}

FunctionType::FunctionType(utl::small_vector<Type const*> argumentTypes,
                           Type const* returnType):
    Type(EntityType::FunctionType, ScopeKind::Type,
         makeFunctionTypeName(argumentTypes, returnType), nullptr, nullptr,
         computeFnTypeAccCtrl(argumentTypes, returnType)),
    _argumentTypes(std::move(argumentTypes)),
    _returnType(returnType) {}

void ObjectType::setLifetimeMetadata(LifetimeMetadata md) {
    if (!lifetimeMD) {
        lifetimeMD = std::make_unique<LifetimeMetadata>(md);
    }
    else {
        *lifetimeMD = md;
    }
}

ObjectType::~ObjectType() = default;

ObjectType::ObjectType(EntityType entityType, ScopeKind scopeKind,
                       std::string name, Scope* parent, size_t size,
                       size_t align, ast::ASTNode* astNode,
                       AccessControl accessControl):
    Type(entityType, scopeKind, std::move(name), parent, astNode,
         accessControl),
    _size(size),
    _align(align) {}

BuiltinType::BuiltinType(EntityType entityType, std::string name,
                         Scope* parentScope, size_t size, size_t align,
                         AccessControl accessControl):
    ObjectType(entityType, ScopeKind::Type, std::move(name), parentScope, size,
               align, nullptr, accessControl) {}

VoidType::VoidType(Scope* parentScope):
    BuiltinType(EntityType::VoidType, "void", parentScope, InvalidSize,
                InvalidSize, AccessControl::Public) {}

ArithmeticType::ArithmeticType(EntityType entityType, std::string name,
                               size_t bitwidth, Signedness signedness,
                               Scope* parentScope):
    BuiltinType(entityType, std::move(name), parentScope,
                utl::ceil_divide(bitwidth, 8), utl::ceil_divide(bitwidth, 8),
                AccessControl::Public),
    _signed(signedness),
    _bitwidth(utl::narrow_cast<uint16_t>(bitwidth)) {}

BoolType::BoolType(Scope* parentScope):
    ArithmeticType(EntityType::BoolType, "bool", 1, Signedness::Unsigned,
                   parentScope) {}

ByteType::ByteType(Scope* parentScope):
    ArithmeticType(EntityType::ByteType, "byte", 8, Signedness::Unsigned,
                   parentScope) {}

static std::string makeIntName(size_t bitwidth, Signedness signedness) {
    switch (signedness) {
    case Signedness::Signed:
        return utl::strcat("s", bitwidth);
    case Signedness::Unsigned:
        return utl::strcat("u", bitwidth);
    }
    SC_UNREACHABLE();
}

IntType::IntType(size_t bitwidth, Signedness signedness, Scope* parentScope):
    ArithmeticType(EntityType::IntType, makeIntName(bitwidth, signedness),
                   bitwidth, signedness, parentScope) {}

static std::string makeFloatName(size_t bitwidth) {
    return utl::strcat("f", bitwidth);
}

FloatType::FloatType(size_t bitwidth, Scope* parentScope):
    ArithmeticType(EntityType::FloatType, makeFloatName(bitwidth), bitwidth,
                   Signedness::Signed, parentScope) {
    SC_ASSERT(bitwidth == 32 || bitwidth == 64, "Invalid width");
}

NullPtrType::NullPtrType(Scope* parent):
    BuiltinType(EntityType::NullPtrType, "__nullptr_t", parent, 1, 1,
                AccessControl::Public) {}

RecordType::RecordType(EntityType entityType, std::string name,
                       Scope* parentScope, ast::ASTNode* astNode, size_t size,
                       size_t align, AccessControl accessControl):
    CompoundType(entityType, ScopeKind::Type, std::move(name), parentScope,
                 size, align, astNode, accessControl) {}

RecordType::~RecordType() = default;

void RecordType::pushBaseObject(BaseClassObject* obj) {
    if (isa<ProtocolType>(obj->type())) {
        obj->_index = _structBaseBegin;
        _elements.insert(_elements.begin() + _structBaseBegin++, obj);
        ++_variableBegin;
    }
    else {
        obj->_index = _variableBegin;
        _elements.insert(_elements.begin() + _variableBegin++, obj);
    }
}

void RecordType::setVTable(std::unique_ptr<VTable> vtable) {
    _vtable = std::move(vtable);
}

void RecordType::setElement(size_t index, Object* obj) {
    if (index >= _elements.size()) {
        _elements.resize(index + 1);
    }
    // clang-format off
    SC_MATCH (*obj) {
        [&](Variable& var) { _elements[index] = &var; },
        [&](BaseClassObject& base) {
            _elements[index] = &base;
            _variableBegin = std::max(_variableBegin, 
                                      utl::narrow_cast<uint16_t>(index + 1));
            if (isa<ProtocolType>(base.type())) {
                _structBaseBegin =
                    std::max(_structBaseBegin,
                             utl::narrow_cast<uint16_t>(index + 1));
            }
        },
        [&](Object const&) { SC_UNREACHABLE(); }
    }; // clang-format on
}

void StructType::pushMemberVariable(Variable* var) {
    var->_index = _elements.size();
    _elements.push_back(var);
}

static Scope* getParent(ObjectType const* type) {
    return type ? const_cast<Scope*>(type->parent()) : nullptr;
}

static size_t computeArraySize(ObjectType* elementType, size_t count) {
    if (!elementType || count == ArrayType::DynamicCount) {
        return InvalidSize;
    }
    return count * elementType->size();
}

static size_t computeArrayAlign(ObjectType* elementType) {
    if (!elementType) {
        return InvalidSize;
    }
    return elementType->align();
}

static std::string makeArrayName(ObjectType const* elemType, size_t count) {
    std::stringstream sstr;
    sstr << "[" << getTypename(elemType);
    if (count != ArrayType::DynamicCount) {
        sstr << "," << count;
    }
    sstr << "]";
    return std::move(sstr).str();
}

ArrayType::ArrayType(ObjectType* elementType, size_t count):
    CompoundType(EntityType::ArrayType, ScopeKind::Type,
                 makeArrayName(elementType, count), getParent(elementType),
                 computeArraySize(elementType, count),
                 computeArrayAlign(elementType), nullptr,
                 elementType->accessControl()),
    elemType(elementType),
    _count(count) {}

void ArrayType::recomputeSize() {
    setSize(computeArraySize(elementType(), count()));
    setAlign(computeArrayAlign(elementType()));
}

static size_t ptrSize(QualType base) { return isa<ArrayType>(*base) ? 16 : 8; }

static size_t ptrAlign() { return 8; }

static std::string makeIndirectName(std::string_view indirection,
                                    QualType base) {
    return utl::strcat(indirection, base.qualName());
}

PointerType::PointerType(EntityType entityType, QualType base,
                         std::string name):
    BuiltinType(entityType, std::move(name), getParent(base.get()),
                ptrSize(base), ptrAlign(), base->accessControl()),
    PtrRefTypeBase(base) {}

RawPtrType::RawPtrType(QualType base):
    PointerType(EntityType::RawPtrType, base, makeIndirectName("*", base)) {}

UniquePtrType::UniquePtrType(QualType base):
    PointerType(EntityType::UniquePtrType, base,
                makeIndirectName("*unique ", base)) {}

ReferenceType::ReferenceType(QualType base):
    Type(EntityType::ReferenceType, ScopeKind::Invalid,
         makeIndirectName("&", base), getParent(base.get()), nullptr,
         base->accessControl()),
    PtrRefTypeBase(base) {}

Function::Function(std::string name, FunctionType const* type,
                   Scope* parentScope, FunctionAttribute attrs,
                   ast::ASTNode* astNode, AccessControl accessControl):
    Scope(EntityType::Function, ScopeKind::Function, std::move(name),
          parentScope, astNode),
    _type(type),
    attrs(attrs) {
    setAccessControl(accessControl);
}

Type const* Function::returnType() const {
    SC_EXPECT(type());
    return type()->returnType();
}

std::span<Type const* const> Function::argumentTypes() const {
    SC_EXPECT(type());
    return type()->argumentTypes();
}

Type const* Function::argumentType(size_t index) const {
    SC_EXPECT(type());
    return type()->argumentType(index);
}

size_t Function::argumentCount() const {
    SC_EXPECT(type());
    return type()->argumentCount();
}

OverloadSet::OverloadSet(SourceRange loc,
                         utl::small_vector<Function*> functions):
    Entity(EntityType::OverloadSet, std::string{}, nullptr),
    small_vector(std::move(functions)),
    loc(loc) {
    setAccessControl(ranges::accumulate(*this, AccessControl::Public,
                                        ranges::max, &Entity::accessControl));
}

Alias::Alias(std::string name, Entity& aliased, Scope* parent,
             ast::ASTNode* astNode, AccessControl accessControl):
    Entity(EntityType::Alias, std::move(name), parent, astNode),
    _aliased(&aliased) {
    setAccessControl(accessControl);
}

PoisonEntity::PoisonEntity(ast::Identifier* ID, EntityCategory cat,
                           Scope* parentScope, AccessControl accessControl):
    Entity(EntityType::PoisonEntity, std::string(ID->value()), parentScope, ID),
    cat(cat) {
    setAccessControl(accessControl);
}
