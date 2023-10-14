#include "Sema/SymbolTable.h"

#include <svm/Builtin.h>
#include <utl/function_view.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>
#include <yaml-cpp/yaml.h>

#include "AST/AST.h"
#include "Common/Ranges.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
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
    Scope* currentScope = nullptr;

    /// Owning list of all entities in this symbol table
    utl::vector<UniquePtr<Entity>> entities;

    /// Map of instantiated `RawPtrType`'s
    utl::hashmap<QualType, RawPtrType*> ptrTypes;

    /// Map of instantiated `ReferenceType`'s
    utl::hashmap<QualType, ReferenceType*> refTypes;

    /// Map of instantiated `UniquePtrType`'s
    utl::hashmap<QualType, UniquePtrType*> uniquePtrTypes;

    /// Map of instantiated `ArrayTypes`'s
    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        arrayTypes;

    /// List of all functions
    utl::small_vector<Function*> functions;

    /// List of all builtin functions
    utl::vector<Function*> builtinFunctions;

    /// The global scope
    GlobalScope* globalScope = nullptr;

    /// ID counter for temporaries
    size_t temporaryID = 0;

    /// The issue handler
    IssueHandler* iss = nullptr;

    /// Conveniece wrapper to emit issues
    /// The same function exists in `AnalysisContext`, maybe we can merge them
    template <typename I, typename... Args>
    void issue(Args&&... args) {
        SC_ASSERT(iss, "Forget to set issue handler?");
        iss->push<I>(currentScope, std::forward<Args>(args)...);
    }

    /// Direct accessors to builtin types
    VoidType* Void;
    ByteType* Byte;
    BoolType* Bool;
    IntType* S8;
    IntType* S16;
    IntType* S32;
    IntType* S64;
    IntType* U8;
    IntType* U16;
    IntType* U32;
    IntType* U64;
    FloatType* F32;
    FloatType* F64;
    ArrayType* Str;
    sema::NullPtrType* NullPtrType;

    template <typename E, typename... Args>
    E* addEntity(Args&&... args);

    template <typename T>
    T* ptrLikeImpl(utl::hashmap<QualType, T*>& map,
                   QualType pointee,
                   utl::function_view<void(T*)> continuation = {});
};

SymbolTable::SymbolTable(): impl(std::make_unique<Impl>()) {
    impl->currentScope = impl->globalScope = impl->addEntity<GlobalScope>();

    using enum Signedness;
    impl->Void = declareBuiltinType<VoidType>();
    impl->Byte = declareBuiltinType<ByteType>();
    impl->Bool = declareBuiltinType<BoolType>();
    impl->S8 = declareBuiltinType<IntType>(8u, Signed);
    impl->S16 = declareBuiltinType<IntType>(16u, Signed);
    impl->S32 = declareBuiltinType<IntType>(32u, Signed);
    impl->S64 = declareBuiltinType<IntType>(64u, Signed);
    impl->U8 = declareBuiltinType<IntType>(8u, Unsigned);
    impl->U16 = declareBuiltinType<IntType>(16u, Unsigned);
    impl->U32 = declareBuiltinType<IntType>(32u, Unsigned);
    impl->U64 = declareBuiltinType<IntType>(64u, Unsigned);
    impl->F32 = declareBuiltinType<FloatType>(32u);
    impl->F64 = declareBuiltinType<FloatType>(64u);
    impl->Str = const_cast<ArrayType*>(arrayType(Byte()));
    impl->NullPtrType = declareBuiltinType<sema::NullPtrType>();

    impl->S64->addAlternateName("int");
    impl->F32->addAlternateName("float");
    impl->F64->addAlternateName("double");
    impl->Str->addAlternateName("str");

    /// Declare builtin functions
    impl->builtinFunctions.resize(static_cast<size_t>(svm::Builtin::_count));
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
    globalScope().addChild(reinterpret);
}

SymbolTable::SymbolTable(SymbolTable&&) noexcept = default;

SymbolTable& SymbolTable::operator=(SymbolTable&&) noexcept = default;

SymbolTable::~SymbolTable() = default;

FileScope* SymbolTable::declareFileScope(std::string filename) {
    auto* file = impl->addEntity<FileScope>(filename, &globalScope());
    globalScope().addChild(file);
    return file;
}

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
    auto* type = impl->addEntity<StructType>(name, &currentScope(), def);
    addToCurrentScope(type);
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
    globalScope().addChild(type);
    type->setBuiltin();
    return type;
}

Function* SymbolTable::declareFuncImpl(ast::FunctionDefinition* def,
                                       std::string name) {
    /// FIXME: This is a quick and dirty solution, we need to find a more
    /// general way to handle this
    if (isKeyword(name) && name != "move") {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    OverloadSet* overloadSet = [&]() -> OverloadSet* {
        auto* entity = currentScope().findEntity(name);
        if (!entity) {
            auto* overloadSet =
                impl->addEntity<OverloadSet>(name, &currentScope());
            addToCurrentScope(overloadSet);
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
                                                   FunctionAttribute::None,
                                                   def);
    addToCurrentScope(function);
    impl->functions.push_back(function);
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
            impl->builtinFunctions[index] = &function;
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
    auto* variable = impl->addEntity<Variable>(name, &currentScope(), vardecl);
    addToCurrentScope(variable);
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

Property* SymbolTable::addProperty(PropertyKind kind,
                                   Type const* type,
                                   Mutability mut,
                                   ValueCategory valueCat) {
    auto* prop =
        impl->addEntity<Property>(kind, &currentScope(), type, mut, valueCat);
    addToCurrentScope(prop);
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
    addToCurrentScope(poison);
}

Scope* SymbolTable::addAnonymousScope() {
    auto* scope =
        impl->addEntity<AnonymousScope>(currentScope().kind(), &currentScope());
    addToCurrentScope(scope);
    return scope;
}

ArrayType const* SymbolTable::arrayType(ObjectType const* elementType,
                                        size_t size) {
    std::pair key = { elementType, size };
    auto itr = impl->arrayTypes.find(key);
    if (itr != impl->arrayTypes.end()) {
        return itr->second;
    }
    /// Const casting is fine because
    /// - the symbol table is the only factory of types so we can guarantee that
    ///   all types are inherently mutable and
    /// - we return the array type as const and the `elementType()` accessor on
    ///   `ArrayType` propagates const-ness.
    auto* arrayType =
        impl->addEntity<ArrayType>(const_cast<ObjectType*>(elementType), size);
    impl->arrayTypes.insert({ key, arrayType });
    withScopeCurrent(arrayType, [&] {
        using enum Mutability;
        using enum ValueCategory;
        auto* arraySize =
            addProperty(PropertyKind::ArraySize, S64(), Const, RValue);
        if (size != ArrayType::DynamicCount) {
            auto constSize = allocate<IntValue>(APInt(size, 64),
                                                /* isSigned = */ true);
            arraySize->setConstantValue(std::move(constSize));
        }
        auto* arrayEmpty =
            addProperty(PropertyKind::ArrayEmpty, Bool(), Const, RValue);
        if (size != ArrayType::DynamicCount) {
            auto constEmpty = allocate<IntValue>(APInt(size == 0, 1),
                                                 /* isSigned = */ false);
            arrayEmpty->setConstantValue(std::move(constEmpty));
        }
        addProperty(PropertyKind::ArrayFront,
                    arrayType->elementType(),
                    Mutable,
                    LValue);
        addProperty(PropertyKind::ArrayBack,
                    arrayType->elementType(),
                    Mutable,
                    LValue);
    });
    declareSpecialLifetimeFunctions(*arrayType, *this);
    const_cast<ObjectType*>(elementType)->parent()->addChild(arrayType);
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

template <typename T>
T* SymbolTable::Impl::ptrLikeImpl(utl::hashmap<QualType, T*>& map,
                                  QualType pointee,
                                  utl::function_view<void(T*)> continuation) {
    auto itr = map.find(pointee);
    if (itr != map.end()) {
        return itr->second;
    }
    auto* ptrType = addEntity<T>(pointee);
    map.insert({ pointee, ptrType });
    const_cast<ObjectType*>(pointee.get())->parent()->addChild(ptrType);
    if (continuation) {
        continuation(ptrType);
    }
    return ptrType;
}

RawPtrType const* SymbolTable::pointer(QualType pointee) {
    return impl->ptrLikeImpl(impl->ptrTypes, pointee);
}

ReferenceType const* SymbolTable::reference(QualType referred) {
    return impl->ptrLikeImpl(impl->refTypes, referred);
}

UniquePtrType const* SymbolTable::uniquePointer(QualType pointee) {
    return impl->ptrLikeImpl<UniquePtrType>(impl->uniquePtrTypes,
                                            pointee,
                                            [&](UniquePtrType* type) {
        declareSpecialLifetimeFunctions(*type, *this);
    });
}

void SymbolTable::pushScope(Scope* scope) {
    SC_ASSERT(currentScope().isChildScope(scope),
              "Scope must be a child of the current scope");
    impl->currentScope = scope;
}

bool SymbolTable::pushScope(std::string_view name) {
    auto* scope = currentScope().findEntity<Scope>(name);
    if (!scope) {
        return false;
    }
    pushScope(scope);
    return true;
}

void SymbolTable::popScope() { impl->currentScope = currentScope().parent(); }

void SymbolTable::makeScopeCurrent(Scope* scope) {
    impl->currentScope = scope ? scope : &globalScope();
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
    return impl->builtinFunctions[index];
}

void SymbolTable::setIssueHandler(IssueHandler& issueHandler) {
    impl->iss = &issueHandler;
}

Scope& SymbolTable::currentScope() { return *impl->currentScope; }

Scope const& SymbolTable::currentScope() const { return *impl->currentScope; }

GlobalScope& SymbolTable::globalScope() { return *impl->globalScope; }

GlobalScope const& SymbolTable::globalScope() const {
    return *impl->globalScope;
}

VoidType const* SymbolTable::Void() const { return impl->Void; }

ByteType const* SymbolTable::Byte() const { return impl->Byte; }

BoolType const* SymbolTable::Bool() const { return impl->Bool; }

IntType const* SymbolTable::S8() const { return impl->S8; }

IntType const* SymbolTable::S16() const { return impl->S16; }

IntType const* SymbolTable::S32() const { return impl->S32; }

IntType const* SymbolTable::S64() const { return impl->S64; }

IntType const* SymbolTable::U8() const { return impl->U8; }

IntType const* SymbolTable::U16() const { return impl->U16; }

IntType const* SymbolTable::U32() const { return impl->U32; }

IntType const* SymbolTable::U64() const { return impl->U64; }

FloatType const* SymbolTable::F32() const { return impl->F32; }

FloatType const* SymbolTable::F64() const { return impl->F64; }

ArrayType const* SymbolTable::Str() const { return impl->Str; }

NullPtrType const* SymbolTable::NullPtrType() const {
    return impl->NullPtrType;
}

std::span<Function* const> SymbolTable::functions() { return impl->functions; }

std::span<Function const* const> SymbolTable::functions() const {
    return impl->functions;
}

std::vector<Entity const*> SymbolTable::entities() const {
    return impl->entities | ToConstAddress | ranges::to<std::vector>;
}

template <typename E, typename... Args>
E* SymbolTable::Impl::addEntity(Args&&... args) {
    auto owner = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    entities.push_back(std::move(owner));
    return result;
}

void SymbolTable::addToCurrentScope(Entity* entity) {
    currentScope().addChild(entity);
    using enum AccessSpecifier;
    bool isPublic = entity->accessSpec().value_or(Public) == Public;
    // FIXME: Remove || true
    if (isa<FileScope>(&currentScope()) && (isPublic || true)) {
        globalScope().addChild(entity);
    }
}

std::string SymbolTable::serialize() const {
    YAML::Node root;
    SC_UNIMPLEMENTED();
}

SymbolTable SymbolTable::deserialize(std::string_view data) {
    SC_UNIMPLEMENTED();
}
