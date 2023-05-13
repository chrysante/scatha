#include "Sema/SymbolTable.h"

#include <svm/Builtin.h>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Common/UniquePtr.h"
#include "Sema/Entity.h"
#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

static bool isKeyword(std::string_view id) {
    static constexpr std::array keywords{
#define SC_KEYWORD_TOKEN_DEF(Token, str) std::string_view(str),
#include "Parser/Token.def"
    };
    return std::find(keywords.begin(), keywords.end(), id) != keywords.end();
}

struct SymbolTable::Impl {
    GlobalScope* _globalScope = nullptr;
    Scope* _currentScope      = nullptr;

    u64 _idCounter = 1;

    utl::vector<UniquePtr<Entity>> _entities;

    utl::hashmap<std::tuple<ObjectType const*, Reference, Mutability>,
                 QualType const*>
        _qualTypes;

    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        _arrayTypes;

    std::vector<StructureType*> _structOrder;

    utl::small_vector<Function*> _functions;

    utl::vector<Function*> _builtinFunctions;

    /// Builtin types
    VoidType* _void;
    ByteType* _byte;
    BoolType* _bool;
    IntType* _s8;
    IntType* _s16;
    IntType* _s32;
    IntType* _s64;
    IntType* _u8;
    IntType* _u16;
    IntType* _u32;
    IntType* _u64;
    FloatType* _f32;
    FloatType* _f64;
};

SymbolTable::SymbolTable(): impl(std::make_unique<Impl>()) {
    impl->_currentScope = impl->_globalScope = addEntity<GlobalScope>();

    using enum Signedness;
    impl->_void = declareBuiltinType<VoidType>();
    impl->_byte = declareBuiltinType<ByteType>();
    impl->_bool = declareBuiltinType<BoolType>();
    impl->_s8   = declareBuiltinType<IntType>(8, Signed);
    impl->_s16  = declareBuiltinType<IntType>(16, Signed);
    impl->_s32  = declareBuiltinType<IntType>(32, Signed);
    impl->_s64  = declareBuiltinType<IntType>(64, Signed);
    impl->_u8   = declareBuiltinType<IntType>(8, Unsigned);
    impl->_u16  = declareBuiltinType<IntType>(16, Unsigned);
    impl->_u32  = declareBuiltinType<IntType>(32, Unsigned);
    impl->_u64  = declareBuiltinType<IntType>(64, Unsigned);
    impl->_f32  = declareBuiltinType<FloatType>(32);
    impl->_f64  = declareBuiltinType<FloatType>(64);

    impl->_s64->addAlternateName("int");
    impl->_f32->addAlternateName("float");
    impl->_f64->addAlternateName("double");

    /// Declare builtin functions
    impl->_builtinFunctions.resize(static_cast<size_t>(svm::Builtin::_count));
#define SVM_BUILTIN_DEF(name, attrs, ...)                                      \
    declareSpecialFunction(                                                    \
        FunctionKind::External,                                                \
        "__builtin_" #name,                                                    \
        /* slot = */ svm::BuiltinFunctionSlot,                                 \
        /* index = */ static_cast<size_t>(svm::Builtin::name),                 \
        FunctionSignature(__VA_ARGS__),                                        \
        attrs);
    using enum FunctionAttribute;
#include <svm/Builtin.def>

    /// Declare builtin generics
    auto* reinterpret = addEntity<Generic>("reinterpret", 1, &globalScope());
    globalScope().add(reinterpret);
}

SymbolTable::SymbolTable(SymbolTable&&) noexcept = default;

SymbolTable& SymbolTable::operator=(SymbolTable&&) noexcept = default;

SymbolTable::~SymbolTable() = default;

Expected<StructureType&, SemanticIssue*> SymbolTable::declareStructureType(
    std::string name, bool allowKeywords) {
    using enum InvalidDeclaration::Reason;
    if (!allowKeywords && isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope());
    }
    if (Entity* entity = currentScope().findEntity(name)) {
        return new InvalidDeclaration(nullptr, Redefinition, currentScope());
    }
    auto* type = addEntity<StructureType>(name, &currentScope());
    currentScope().add(type);
    return *type;
}

template <typename T, typename... Args>
T* SymbolTable::declareBuiltinType(Args&&... args) {
    auto* type = addEntity<T>(std::forward<Args>(args)..., &currentScope());
    currentScope().add(type);
    return type;
}

Expected<Function&, SemanticIssue*> SymbolTable::declareFunction(
    std::string name) {
    using enum InvalidDeclaration::Reason;
    if (isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope());
    }
    OverloadSet* overloadSet = [&] {
        if (auto* entity = currentScope().findEntity(name)) {
            return dyncast<OverloadSet*>(entity);
        }
        /// Create a new overload set
        auto* overloadSet = addEntity<OverloadSet>(name, &currentScope());
        currentScope().add(overloadSet);
        return overloadSet;
    }();
    /// We still don't know if the `dyncast` in the lambda was successful
    if (!overloadSet) {
        return new InvalidDeclaration(nullptr,
                                      InvalidDeclaration::Reason::Redefinition,
                                      currentScope());
    }
    Function* function = addEntity<Function>(name,
                                             overloadSet,
                                             &currentScope(),
                                             FunctionAttribute::None);
    currentScope().add(function);
    impl->_functions.push_back(function);
    return *function;
}

Expected<void, SemanticIssue*> SymbolTable::setSignature(
    Function* function, FunctionSignature sig) {
    function->setSignature(std::move(sig));
    auto* overloadSet                   = function->overloadSet();
    auto const [otherFunction, success] = overloadSet->add(function);
    if (!success) {
        using enum InvalidDeclaration::Reason;
        InvalidDeclaration::Reason const reason =
            otherFunction->signature().returnType() ==
                    function->signature().returnType() ?
                Redefinition :
                CantOverloadOnReturnType;
        return new InvalidDeclaration(nullptr, reason, currentScope());
    }
    return {};
}

bool SymbolTable::declareSpecialFunction(FunctionKind kind,
                                         std::string name,
                                         size_t slot,
                                         size_t index,
                                         FunctionSignature signature,
                                         FunctionAttribute attrs) {
    withScopeCurrent(&globalScope(), [&] {
        auto declResult = declareFunction(std::move(name));
        if (!declResult) {
            return false;
        }
        auto& function = *declResult;
        setSignature(&function, std::move(signature));
        function._kind  = kind;
        function.attrs  = attrs;
        function._slot  = utl::narrow_cast<u16>(slot);
        function._index = utl::narrow_cast<u32>(index);
        if (kind == FunctionKind::External && slot == svm::BuiltinFunctionSlot)
        {
            impl->_builtinFunctions[index] = &function;
        }
        return true;
    });
}

Expected<Variable&, SemanticIssue*> SymbolTable::declareVariable(
    std::string name) {
    using enum InvalidDeclaration::Reason;
    if (isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope());
    }
    if (auto* entity = currentScope().findEntity(name)) {
        return new InvalidDeclaration(nullptr, Redefinition, currentScope());
    }
    auto* variable = addEntity<Variable>(name, &currentScope());
    currentScope().add(variable);
    return *variable;
}

Expected<Variable&, SemanticIssue*> SymbolTable::addVariable(
    std::string name, QualType const* type) {
    auto declResult = declareVariable(std::move(name));
    if (!declResult) {
        return declResult.error();
    }
    auto& var = *declResult;
    var.setType(type);
    return var;
}

Expected<PoisonEntity&, SemanticIssue*> SymbolTable::declarePoison(
    std::string name, EntityCategory cat) {
    using enum InvalidDeclaration::Reason;
    if (isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope());
    }
    if (auto* entity = currentScope().findEntity(name)) {
        return new InvalidDeclaration(nullptr, Redefinition, currentScope());
    }
    auto* entity = addEntity<PoisonEntity>(name, cat, &currentScope());
    currentScope().add(entity);
    return *entity;
}

Scope& SymbolTable::addAnonymousScope() {
    auto* scope =
        addEntity<AnonymousScope>(currentScope().kind(), &currentScope());
    currentScope().add(scope);
    return *scope;
}

ArrayType const* SymbolTable::arrayType(ObjectType const* elementType,
                                        size_t size) {
    std::pair key = { elementType, size };
    auto itr      = impl->_arrayTypes.find(key);
    if (itr != impl->_arrayTypes.end()) {
        return itr->second;
    }
    auto* arrayType = addEntity<ArrayType>(elementType, size);
    impl->_arrayTypes.insert({ key, arrayType });
    withScopeCurrent(arrayType, [&] {
        auto* countVar = &addVariable("count", qS64()).value();
        countVar->setIndex(1);
        arrayType->setCountVariable(countVar);
    });
    return arrayType;
}

QualType const* SymbolTable::qualify(ObjectType const* base,
                                     Reference ref,
                                     Mutability mut) {
    std::tuple key = { base, ref, mut };
    auto itr       = impl->_qualTypes.find(key);
    if (itr != impl->_qualTypes.end()) {
        return itr->second;
    }
    auto* qualType =
        addEntity<QualType>(const_cast<ObjectType*>(base), ref, mut);
    impl->_qualTypes.insert({ key, qualType });
    return qualType;
}

QualType const* SymbolTable::stripQualifiers(QualType const* type) {
    return qualify(type->base());
}

QualType const* SymbolTable::copyQualifiers(QualType const* from,
                                            ObjectType const* to) {
    return qualify(to, from->reference());
}

QualType const* SymbolTable::setReference(QualType const* type, Reference ref) {
    return qualify(type->base(), ref, type->mutability());
}

QualType const* SymbolTable::setMutable(QualType const* type, Mutability mut) {
    return qualify(type->base(), type->reference(), mut);
}

QualType const* SymbolTable::qDynArray(ObjectType const* base,
                                       Reference ref,
                                       Mutability mut) {
    return qualify(arrayType(base, ArrayType::DynamicCount), ref, mut);
}

void SymbolTable::pushScope(Scope* scope) {
    SC_ASSERT(currentScope().isChildScope(scope),
              "Scope must be a child of the current scope");
    impl->_currentScope = scope;
}

void SymbolTable::popScope() { impl->_currentScope = currentScope().parent(); }

void SymbolTable::makeScopeCurrent(Scope* scope) {
    impl->_currentScope = scope ? scope : &globalScope();
}

Entity const* SymbolTable::lookup(std::string_view name) const {
    Scope const* scope = &currentScope();
    while (scope != nullptr) {
        if (auto* entity = scope->findEntity(name)) {
            return entity;
        }
        scope = scope->parent();
    }
    return nullptr;
}

Function* SymbolTable::builtinFunction(size_t index) const {
    return impl->_builtinFunctions[index];
}

Scope& SymbolTable::currentScope() { return *impl->_currentScope; }

Scope const& SymbolTable::currentScope() const { return *impl->_currentScope; }

GlobalScope& SymbolTable::globalScope() { return *impl->_globalScope; }

GlobalScope const& SymbolTable::globalScope() const {
    return *impl->_globalScope;
}

VoidType const* SymbolTable::Void() const { return impl->_void; }
ByteType const* SymbolTable::Byte() const { return impl->_byte; }
BoolType const* SymbolTable::Bool() const { return impl->_bool; }
IntType const* SymbolTable::S8() const { return impl->_s8; }
IntType const* SymbolTable::S16() const { return impl->_s16; }
IntType const* SymbolTable::S32() const { return impl->_s32; }
IntType const* SymbolTable::S64() const { return impl->_s64; }
IntType const* SymbolTable::U8() const { return impl->_u8; }
IntType const* SymbolTable::U16() const { return impl->_u16; }
IntType const* SymbolTable::U32() const { return impl->_u32; }
IntType const* SymbolTable::U64() const { return impl->_u64; }
FloatType const* SymbolTable::F32() const { return impl->_f32; }
FloatType const* SymbolTable::F64() const { return impl->_f64; }

QualType const* SymbolTable::qVoid(Reference ref) {
    return qualify(Void(), ref);
}

QualType const* SymbolTable::qByte(Reference ref) {
    return qualify(Byte(), ref);
}

QualType const* SymbolTable::qBool(Reference ref) {
    return qualify(Bool(), ref);
}

QualType const* SymbolTable::qS8(Reference ref) { return qualify(S8(), ref); }

QualType const* SymbolTable::qS16(Reference ref) { return qualify(S16(), ref); }

QualType const* SymbolTable::qS32(Reference ref) { return qualify(S32(), ref); }

QualType const* SymbolTable::qS64(Reference ref) { return qualify(S64(), ref); }

QualType const* SymbolTable::qU8(Reference ref) { return qualify(U8(), ref); }

QualType const* SymbolTable::qU16(Reference ref) { return qualify(U16(), ref); }

QualType const* SymbolTable::qU32(Reference ref) { return qualify(U32(), ref); }

QualType const* SymbolTable::qU64(Reference ref) { return qualify(U64(), ref); }

QualType const* SymbolTable::qF32(Reference ref) { return qualify(F32(), ref); }

QualType const* SymbolTable::qF64(Reference ref) { return qualify(F64(), ref); }

IntType const* SymbolTable::intType(size_t width, Signedness signedness) {
    bool isSigned = signedness == Signedness::Signed;
    switch (width) {
    case 8:
        return isSigned ? S8() : U8();
    case 16:
        return isSigned ? S16() : U16();
    case 32:
        return isSigned ? S32() : U32();
    case 64:
        return isSigned ? S64() : U64();
    default:
        SC_UNREACHABLE();
    }
}

void SymbolTable::setStructDependencyOrder(
    std::vector<StructureType*> structs) {
    impl->_structOrder = std::move(structs);
}

/// View over topologically sorted struct types
std::span<StructureType const* const> SymbolTable::structDependencyOrder()
    const {
    return impl->_structOrder;
}

std::span<Function const* const> SymbolTable::functions() const {
    return impl->_functions;
}

template <typename E, typename... Args>
E* SymbolTable::addEntity(Args&&... args) {
    auto owner   = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    impl->_entities.push_back(std::move(owner));
    return result;
}
