#include "Sema/SymbolTable.h"

#include <svm/Builtin.h>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"

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
    /// The currently active scope
    Scope* _currentScope = nullptr;

    /// Owning list of all entities in this symbol table
    utl::vector<UniquePtr<Entity>> _entities;

    /// Map of instantiated `PointerType`'s
    utl::hashmap<QualType, PointerType const*> ptrTypes;

    /// Map of instantiated `ReferenceType`'s
    utl::hashmap<QualType, ReferenceType const*> refTypes;

    /// Map of instantiated `ArrayTypes`'s
    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        _arrayTypes;

    /// List of all functions
    utl::small_vector<Function*> _functions;

    /// List of all builtin functions
    utl::vector<Function*> _builtinFunctions;

    /// The global scope
    GlobalScope* _globalScope = nullptr;

    /// ID counter for temporaries
    size_t temporaryID = 0;

    /// The issue handler
    IssueHandler* iss = nullptr;

    /// Conveniece wrapper to emit issues
    /// The same function exists in `AnalysisContext`, maybe we can merge them
    template <typename I, typename... Args>
    void issue(Args&&... args) {
        SC_ASSERT(iss, "Forget to set issue handler?");
        iss->push<I>(_currentScope, std::forward<Args>(args)...);
    }

    /// Direct accessors to builtin types
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
    ArrayType* _str;

    template <typename E, typename... Args>
    E* addEntity(Args&&... args);
};

SymbolTable::SymbolTable(): impl(std::make_unique<Impl>()) {
    impl->_currentScope = impl->_globalScope = impl->addEntity<GlobalScope>();

    using enum Signedness;
    impl->_void = declareBuiltinType<VoidType>();
    impl->_byte = declareBuiltinType<ByteType>();
    impl->_bool = declareBuiltinType<BoolType>();
    impl->_s8 = declareBuiltinType<IntType>(8u, Signed);
    impl->_s16 = declareBuiltinType<IntType>(16u, Signed);
    impl->_s32 = declareBuiltinType<IntType>(32u, Signed);
    impl->_s64 = declareBuiltinType<IntType>(64u, Signed);
    impl->_u8 = declareBuiltinType<IntType>(8u, Unsigned);
    impl->_u16 = declareBuiltinType<IntType>(16u, Unsigned);
    impl->_u32 = declareBuiltinType<IntType>(32u, Unsigned);
    impl->_u64 = declareBuiltinType<IntType>(64u, Unsigned);
    impl->_f32 = declareBuiltinType<FloatType>(32u);
    impl->_f64 = declareBuiltinType<FloatType>(64u);
    impl->_str = const_cast<ArrayType*>(arrayType(Byte()));

    impl->_s64->addAlternateName("int");
    impl->_f32->addAlternateName("float");
    impl->_f64->addAlternateName("double");
    globalScope().add(impl->_str);
    impl->_str->addAlternateName("str");

    /// Declare builtin functions
    impl->_builtinFunctions.resize(static_cast<size_t>(svm::Builtin::_count));
#define SVM_BUILTIN_DEF(name, attrs, ...)                                      \
    declareSpecialFunction(FunctionKind::Foreign,                              \
                           "__builtin_" #name,                                 \
                           svm::BuiltinFunctionSlot,                           \
                           static_cast<size_t>(svm::Builtin::name),            \
                           FunctionSignature(__VA_ARGS__),                     \
                           attrs);
    using enum FunctionAttribute;
#include <svm/Builtin.def>

    /// Declare builtin generics
    auto* reinterpret =
        impl->addEntity<Generic>("reinterpret", 1u, &globalScope());
    reinterpret->setBuiltin();
    globalScope().add(reinterpret);
}

SymbolTable::SymbolTable(SymbolTable&&) noexcept = default;

SymbolTable& SymbolTable::operator=(SymbolTable&&) noexcept = default;

SymbolTable::~SymbolTable() = default;

StructType* SymbolTable::declareStructImpl(ast::StructDefinition* def,
                                           std::string name) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (Entity* entity = currentScope().findEntity(name)) {
        impl->issue<Redefinition>(def, entity);
        return nullptr;
    }
    auto* type = impl->addEntity<StructType>(name, &currentScope());
    currentScope().add(type);
    return type;
}

StructType* SymbolTable::declareStructureType(ast::StructDefinition* def) {
    return declareStructImpl(def, std::string(def->name()));
}

StructType* SymbolTable::declareStructureType(std::string name) {
    return declareStructImpl(nullptr, std::move(name));
}

template <typename T, typename... Args>
T* SymbolTable::declareBuiltinType(Args&&... args) {
    auto* type =
        impl->addEntity<T>(std::forward<Args>(args)..., &currentScope());
    currentScope().add(type);
    type->setBuiltin();
    return type;
}

Function* SymbolTable::declareFuncImpl(ast::FunctionDefinition* def,
                                       std::string name) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    OverloadSet* overloadSet = [&]() -> OverloadSet* {
        auto* entity = currentScope().findEntity(name);
        if (!entity) {
            auto* overloadSet =
                impl->addEntity<OverloadSet>(name, &currentScope());
            currentScope().add(overloadSet);
            return overloadSet;
        }
        if (auto* os = dyncast<OverloadSet*>(entity)) {
            return os;
        }
        impl->issue<Redefinition>(def, entity);
        return nullptr;
    }();
    if (!overloadSet) {
        return nullptr;
    }
    Function* function = impl->addEntity<Function>(name,
                                                   overloadSet,
                                                   &currentScope(),
                                                   FunctionAttribute::None);
    currentScope().add(function);
    impl->_functions.push_back(function);
    return function;
}

Function* SymbolTable::declareFuncName(ast::FunctionDefinition* def) {
    return declareFuncImpl(def, std::string(def->name()));
}

Function* SymbolTable::declareFuncName(std::string name) {
    return declareFuncImpl(nullptr, name);
}

bool SymbolTable::setFuncSig(Function* function, FunctionSignature sig) {
    function->setSignature(std::move(sig));
    auto* overloadSet = function->overloadSet();
    auto* existing = overloadSet->add(function);
    if (!existing) {
        return true;
    }
    impl->issue<Redefinition>(cast_or_null<ast::Declaration*>(
                                  function->astNode()),
                              existing);
    return false;
}

bool SymbolTable::declareSpecialFunction(FunctionKind kind,
                                         std::string name,
                                         size_t slot,
                                         size_t index,
                                         FunctionSignature signature,
                                         FunctionAttribute attrs) {
    return withScopeCurrent(&globalScope(), [&] {
        auto declResult = declareFuncName(std::move(name));
        if (!declResult) {
            return false;
        }
        auto& function = *declResult;
        bool success = setFuncSig(&function, std::move(signature));
        SC_ASSERT(success, "Failed to overload function");
        function._kind = kind;
        function.attrs = attrs;
        function._slot = utl::narrow_cast<u16>(slot);
        function._index = utl::narrow_cast<u32>(index);
        if (kind == FunctionKind::Foreign && slot == svm::BuiltinFunctionSlot) {
            function.setBuiltin();
            function.overloadSet()->setBuiltin();
            impl->_builtinFunctions[index] = &function;
        }
        return true;
    });
}

Variable* SymbolTable::declareVarImpl(ast::VarDeclBase* vardecl,
                                      std::string name) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(vardecl,
                                    GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (auto* existing = currentScope().findEntity(name)) {
        impl->issue<Redefinition>(vardecl, existing);
        return nullptr;
    }
    auto* variable = impl->addEntity<Variable>(name, &currentScope());
    currentScope().add(variable);
    return variable;
}

Variable* SymbolTable::defineVarImpl(ast::VarDeclBase* vardecl,
                                     std::string name,
                                     Type const* type,
                                     Mutability mut) {
    auto* var = declareVarImpl(vardecl, std::move(name));
    if (!var) {
        return nullptr;
    }
    var->setType(type);
    var->setMutability(mut);
    return var;
}

Variable* SymbolTable::declareVariable(ast::VarDeclBase* vardecl) {
    return declareVarImpl(vardecl, std::string(vardecl->name()));
}

Variable* SymbolTable::declareVariable(std::string name) {
    return declareVarImpl(nullptr, std::move(name));
}

Variable* SymbolTable::defineVariable(ast::VarDeclBase* vardecl,
                                      Type const* type,
                                      Mutability mut) {
    return defineVarImpl(vardecl, std::string(vardecl->name()), type, mut);
}

Variable* SymbolTable::defineVariable(std::string name,
                                      Type const* type,
                                      Mutability mut) {
    return defineVarImpl(nullptr, std::move(name), type, mut);
}

Property* SymbolTable::addProperty(PropertyKind kind, Type const* type) {
    auto* prop = impl->addEntity<Property>(kind, &currentScope(), type);
    currentScope().add(prop);
    return prop;
}

Temporary* SymbolTable::temporary(QualType type) {
    auto* temp =
        impl->addEntity<Temporary>(impl->temporaryID++, &currentScope(), type);
    return temp;
}

void SymbolTable::declarePoison(std::string name, EntityCategory cat) {
    if (isKeyword(name) || currentScope().findEntity(name)) {
        return;
    }
    auto* poison = impl->addEntity<PoisonEntity>(name, cat, &currentScope());
    currentScope().add(poison);
}

Scope* SymbolTable::addAnonymousScope() {
    auto* scope =
        impl->addEntity<AnonymousScope>(currentScope().kind(), &currentScope());
    currentScope().add(scope);
    return scope;
}

ArrayType const* SymbolTable::arrayType(ObjectType const* elementType,
                                        size_t size) {
    std::pair key = { elementType, size };
    auto itr = impl->_arrayTypes.find(key);
    if (itr != impl->_arrayTypes.end()) {
        return itr->second;
    }
    auto* arrayType = impl->addEntity<ArrayType>(elementType, size);
    impl->_arrayTypes.insert({ key, arrayType });
    withScopeCurrent(arrayType, [&] {
        auto* arraySize = addProperty(PropertyKind::ArraySize, S64());
        arrayType->setCountProperty(arraySize);
    });
    return arrayType;
}

ArrayType const* SymbolTable::arrayType(ObjectType const* elementType) {
    return arrayType(elementType, ArrayType::DynamicCount);
}

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

PointerType const* SymbolTable::pointer(QualType pointee) {
    auto itr = impl->ptrTypes.find(pointee);
    if (itr != impl->ptrTypes.end()) {
        return itr->second;
    }
    auto* ptrType = impl->addEntity<PointerType>(pointee);
    impl->ptrTypes.insert({ pointee, ptrType });
    return ptrType;
}

ReferenceType const* SymbolTable::reference(QualType referred) {
    auto itr = impl->refTypes.find(referred);
    if (itr != impl->refTypes.end()) {
        return itr->second;
    }
    auto* refType = impl->addEntity<ReferenceType>(referred);
    impl->refTypes.insert({ referred, refType });
    return refType;
}

void SymbolTable::pushScope(Scope* scope) {
    SC_ASSERT(currentScope().isChildScope(scope),
              "Scope must be a child of the current scope");
    impl->_currentScope = scope;
}

bool SymbolTable::pushScope(std::string_view name) {
    auto* scope = currentScope().findEntity<Scope>(name);
    if (!scope) {
        return false;
    }
    pushScope(scope);
    return true;
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

void SymbolTable::setIssueHandler(IssueHandler& issueHandler) {
    impl->iss = &issueHandler;
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

ArrayType const* SymbolTable::Str() const { return impl->_str; }

std::span<Function* const> SymbolTable::functions() { return impl->_functions; }

std::span<Function const* const> SymbolTable::functions() const {
    return impl->_functions;
}

std::vector<Entity const*> SymbolTable::entities() const {
    return impl->_entities | ToConstAddress | ranges::to<std::vector>;
}

template <typename E, typename... Args>
E* SymbolTable::Impl::addEntity(Args&&... args) {
    auto owner = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    _entities.push_back(std::move(owner));
    return result;
}
