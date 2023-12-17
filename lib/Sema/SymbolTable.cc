#include "Sema/SymbolTable.h"

#include <optional>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <utl/dynamic_library.hpp>
#include <utl/function_view.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

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
using namespace ranges::views;

static bool isKeyword(std::string_view id) {
    static constexpr std::array keywords{
#define SC_KEYWORD_TOKEN_DEF(Token, str) std::string_view(str),
#include "Parser/Token.def"
    };
    return std::find(keywords.begin(), keywords.end(), id) != keywords.end();
}

/// Discriminator for `checkRedef()`
enum Redef { Redef_Function, Redef_Other };

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

    /// Pathes to search for imported libraries
    std::vector<std::filesystem::path> libSearchPaths;

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
    declareForeignFunction("__builtin_" #name,                                 \
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

SymbolTable::SymbolTable(SymbolTable&& rhs) noexcept { *this = std::move(rhs); }

SymbolTable& SymbolTable::operator=(SymbolTable&& rhs) noexcept {
    impl = std::move(rhs.impl);
    rhs.impl = SymbolTable().impl;
    return *this;
}

SymbolTable::~SymbolTable() = default;

FileScope* SymbolTable::declareFileScope(std::string filename) {
    auto* file = impl->addEntity<FileScope>(filename, &globalScope());
    globalScope().addChild(file);
    return file;
}

static std::optional<std::filesystem::path> findLibrary(
    std::span<std::filesystem::path const> searchPaths, std::string name) {
    for (auto& path: searchPaths) {
        auto fullPath = path / (name + ".dylib");
        if (std::filesystem::exists(fullPath)) {
            return fullPath;
        }
    }
    return std::nullopt;
}

LibraryScope* SymbolTable::importLibrary(ast::ImportStatement* stmt) {
    if (currentScope().kind() != ScopeKind::Global) {
        return nullptr;
    }
    auto* id = dyncast<ast::Identifier const*>(stmt->libExpr());
    if (!id) {
        impl->issue<BadImport>(stmt);
        return nullptr;
    }
    std::string name(id->value());
    auto path = findLibrary(impl->libSearchPaths, name);
    if (!path) {
        impl->issue<BadImport>(stmt);
        return nullptr;
    }
#if 0 // Need to link utl to use dynamic_library
    utl::dynamic_library sharedLib(*path);
    sharedLib.symbol_ptr<void()>("internal_declareFunctions");
    auto* lib = impl->addEntity<LibraryScope>(name, &currentScope());
#endif
    SC_UNIMPLEMENTED();
}

StructType* SymbolTable::declareStructImpl(ast::StructDefinition* def,
                                           std::string name) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Other, name, def)) {
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
    if (!checkRedef(Redef_Function, name, def)) {
        return nullptr;
    }
    Function* function = impl->addEntity<Function>(name,
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

/// \Returns the set of functions with the same name as \p ref in all parent
/// scopes of \p ref that already have a signature
static utl::small_vector<Function const*> findScopeOS(Function const* ref) {
    utl::hashset<Function const*> set;
    for (auto* parent: ref->parents()) {
        auto entities = parent->findEntities(ref->name());
        auto functions = entities | transform(cast<Function const*>) |
                         filter(&Function::hasSignature);
        set.insert(functions.begin(), functions.end());
    }
    set.erase(ref);
    return set | ToSmallVector<>;
}

bool SymbolTable::setFuncSig(Function* function, FunctionSignature sig) {
    function->setSignature(sig); /// We don't move `sig` here because we use it
                                 /// later in this function
    auto overloadSet = findScopeOS(function);
    auto* existing = OverloadSet::find(overloadSet, sig.argumentTypes());
    if (existing) {
        impl->issue<Redefinition>(function->definition(), existing);
        return false;
    }
    return true;
}

Function* SymbolTable::declareFunction(std::string name,
                                       FunctionSignature sig) {
    auto* function = declareFuncImpl(nullptr, std::move(name));
    if (function && setFuncSig(function, std::move(sig))) {
        return function;
    }
    return nullptr;
}

OverloadSet* SymbolTable::addOverloadSet(
    SourceRange sourceRange, utl::small_vector<Function*> functions) {
    SC_EXPECT(!functions.empty());
    auto name = std::string(functions.front()->name());
    return impl->addEntity<OverloadSet>(sourceRange,
                                        std::move(name),
                                        functions);
}

Function* SymbolTable::declareForeignFunction(std::string name,
                                              size_t slot,
                                              size_t index,
                                              FunctionSignature sig,
                                              FunctionAttribute attrs) {
    auto* function = declareFunction(std::move(name), std::move(sig));
    if (!function) {
        return nullptr;
    }
    function->setForeign(slot, index, attrs);
    if (slot == svm::BuiltinFunctionSlot) {
        impl->builtinFunctions[index] = function;
    }
    return function;
}

Variable* SymbolTable::declareVarImpl(ast::VarDeclBase* vardecl,
                                      std::string name) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(vardecl,
                                    GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Other, name, vardecl)) {
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
    if (isKeyword(name) || !currentScope().findEntities(name).empty()) {
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
    if (auto* type = const_cast<ObjectType*>(pointee.get());
        type && type->parent())
    {
        type->parent()->addChild(ptrType);
    }
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

void SymbolTable::popScope() { impl->currentScope = currentScope().parent(); }

void SymbolTable::makeScopeCurrent(Scope* scope) {
    impl->currentScope = scope ? scope : &globalScope();
}

utl::small_vector<Entity*> SymbolTable::unqualifiedLookup(
    std::string_view name) {
    utl::hashset<Entity*> overloadSet;
    for (auto* scope = &currentScope(); scope != nullptr;
         scope = scope->parent())
    {
        auto entities = scope->findEntities(name);
        if (entities.empty()) {
            continue;
        }
        SC_ASSERT(ranges::all_of(entities, isa<Function>) ||
                      ranges::none_of(entities, isa<Function>),
                  "If any entity with this name is a function all must be "
                  "functions");
        /// If we have functions we build up the overload set
        if (isa<Function>(entities.front())) {
            overloadSet.insert(entities.begin(), entities.end());
            continue;
        }
        /// Otherwise if this is the first entity we find we return that
        if (overloadSet.empty()) {
            return entities | ToSmallVector<>;
        }
    }
    return overloadSet | ToSmallVector<>;
}

Function* SymbolTable::builtinFunction(size_t index) const {
    return impl->builtinFunctions[index];
}

void SymbolTable::setIssueHandler(IssueHandler& issueHandler) {
    impl->iss = &issueHandler;
}

void SymbolTable::setLibrarySearchPaths(
    std::span<std::filesystem::path const> paths) {
    impl->libSearchPaths = paths | ranges::to<std::vector>;
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
    if (isa<FileScope>(&currentScope()) && isPublic) {
        globalScope().addChild(entity);
    }
}

static bool checkRedefImpl(SymbolTable::Impl& impl,
                           Redef kind,
                           Scope const* scope,
                           std::string_view name,
                           ast::Declaration const* decl) {
    auto entities = scope->findEntities(name);
    switch (kind) {
    case Redef_Function:
        if (ranges::all_of(entities, isa<Function>)) {
            return true;
        }
        break;
    case Redef_Other:
        if (entities.empty()) {
            return true;
        }
        break;
    }
    impl.issue<Redefinition>(decl, entities.front());
    return false;
}

/// \Returns `true` if no redefinition occured
bool SymbolTable::checkRedef(int kind,
                             std::string_view name,
                             ast::Declaration const* decl) {
    if (!checkRedefImpl(*impl, Redef(kind), &currentScope(), name, decl)) {
        return false;
    }
    if (isa<FileScope>(currentScope()) && decl->isPublic()) {
        return checkRedefImpl(*impl, Redef(kind), &globalScope(), name, decl);
    }
    return true;
}
