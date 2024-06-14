#include "Sema/SymbolTable.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/dynamic_library.hpp>
#include <utl/function_view.hpp>
#include <utl/hash.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/Builtin.h"
#include "Common/FileHandling.h"
#include "Common/Ranges.h"
#include "Common/UniquePtr.h"
#include "Invocation/TargetNames.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeFunctionAnalysis.h"
#include "Sema/SemaIssues.h"
#include "Sema/Serialize.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

static bool isKeyword(std::string_view id) {
    static constexpr std::array keywords{
#define SC_KEYWORD_TOKEN_DEF(Token, str) std::string_view(str),
#include "Parser/Token.def.h"
    };
    return std::find(keywords.begin(), keywords.end(), id) != keywords.end();
}

namespace {

/// Discriminator for `checkRedef()`
enum Redef { Redef_Function, Redef_Other };

/// Comparable  and hashable key for function types
struct FuncSig {
    utl::small_vector<Type const*> argTypes;
    Type const* retType;

    bool operator==(FuncSig const&) const = default;
};

} // namespace

template <>
struct std::hash<FuncSig> {
    size_t operator()(FuncSig const& sig) const {
        return utl::hash_combine(utl::hash_combine_range(sig.argTypes.begin(),
                                                         sig.argTypes.end()),
                                 sig.retType);
    }
};

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

    /// Map of instantiated `FunctionType`'s
    utl::hashmap<FuncSig, FunctionType const*> functionTypes;

    /// Map of instantiated `ArrayTypes`'s
    utl::hashmap<std::pair<ObjectType const*, size_t>, ArrayType const*>
        arrayTypes;

    /// Map of instantiated `TypeDeductionQualifier`'s
    utl::hashmap<std::pair<ReferenceKind, Mutability>, TypeDeductionQualifier*>
        typeDeductionQualifiers;

    /// List of all functions
    utl::small_vector<Function*> functions;

    /// List of all structs
    utl::small_vector<StructType*> structTypes;

    /// List of all imported libraries
    utl::small_vector<Library*> importedLibs;

    /// Name map
    utl::hashmap<std::string, NativeLibrary*> nativeLibMap;

    /// Name map
    utl::hashmap<std::string, ForeignLibrary*> foreignLibMap;

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
        requires std::constructible_from<I, Scope*, Args...>
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
        requires std::constructible_from<E, Args...>
    E* addEntity(Args&&... args);

    template <typename T>
    T* ptrLikeImpl(utl::hashmap<QualType, T*>& map, QualType pointee,
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

    declareAlias("int", *impl->S64, nullptr, AccessControl::Public);
    declareAlias("float", *impl->F32, nullptr, AccessControl::Public);
    declareAlias("double", *impl->F64, nullptr, AccessControl::Public);
    declareAlias("str", *impl->Str, nullptr, AccessControl::Public);

    /// Declare builtin functions
#define SVM_BUILTIN_DEF(name, attrs, ...)                                      \
    declareForeignFunction("__builtin_" #name, functionType(__VA_ARGS__),      \
                           attrs, AccessControl::Public);
    using enum FunctionAttribute;
#include <svm/Builtin.def.h>

    /// Declare builtin generics
    auto* reinterpret =
        impl->addEntity<Generic>("reinterpret", 1u, &globalScope());
    globalScope().addChild(reinterpret);
}

SymbolTable::SymbolTable(SymbolTable&& rhs) noexcept:
    impl(std::make_unique<Impl>()) {
    *this = std::move(rhs);
}

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

static std::optional<std::filesystem::path> findLibraryImpl(
    std::span<std::filesystem::path const> searchPaths, std::string name,
    utl::function_view<bool(std::filesystem::path const&)> exists) {
    if (exists(name)) {
        return std::filesystem::path(name);
    }
    for (auto& path: searchPaths) {
        auto fullPath = path / name;
        if (exists(fullPath)) {
            return fullPath;
        }
    }
    return std::nullopt;
}

static std::optional<std::filesystem::path> findNativeLibrary(
    std::span<std::filesystem::path const> searchPaths, std::string name) {
    return findLibraryImpl(searchPaths, std::move(name),
                           [](std::filesystem::path const& path) {
        return std::filesystem::exists(path);
    });
}

static bool foreignLibPathExists(std::filesystem::path const& path) {
#if defined(__APPLE__) || defined(_WIN32)
    try {
        [[maybe_unused]] auto lib = utl::dynamic_library(path.string());
        return true;
    }
    catch (std::exception const&) {
        return false;
    }
#else
    return std::filesystem::exists(path);
#endif
}

static std::optional<std::filesystem::path> findForeignLibrary(
    std::span<std::filesystem::path const> searchPaths, std::string name) {
    return findLibraryImpl(searchPaths, std::move(name), foreignLibPathExists);
}

static std::string toForeignLibName(std::string_view fullname) {
    /// TODO: Make portable
    /// This is the MacOS convention, need to add linux and windows conventions
    /// for portability
#if defined(__APPLE__)
    std::filesystem::path path(fullname);
    auto name = path.filename().string();
    path.replace_filename(utl::strcat("lib", name, ".dylib"));
#elif defined(_WIN32)
    std::filesystem::path path(fullname);
    auto name = path.filename().string();
    path.replace_filename(utl::strcat(name, ".dll"));
#else
#error Unknown OS
#endif
    return path.string();
}

static ForeignLibrary* importForeignLib(SymbolTable& sym,
                                        SymbolTable::Impl& impl,
                                        ast::ASTNode* astNode,
                                        std::string libname) {
    SC_ASSERT(!impl.foreignLibMap.contains(libname),
              "This library is already imported");
    auto path =
        findForeignLibrary(impl.libSearchPaths, toForeignLibName(libname));
    if (!path) {
        if (astNode) {
            impl.issue<BadImport>(astNode, BadImport::LibraryNotFound);
        }
        else {
            impl.issue<BadImport>(libname);
        }
        return nullptr;
    }
    auto* lib =
        impl.addEntity<ForeignLibrary>(libname, *path, &sym.globalScope());
    /// Temporary measure, because only public symbols are serialized
    lib->setAccessControl(AccessControl::Public);
    /// We add foreign libraries to the global scope because this makes
    /// exporting foreign library imports from native libraries easier. This
    /// does not affect name lookup because foreign libs don't expose names in
    /// the frontend
    sym.globalScope().addChild(lib);
    impl.importedLibs.push_back(lib);
    impl.foreignLibMap.insert({ libname, lib });
    return lib;
}

static NativeLibrary* importNativeLib(SymbolTable& sym, SymbolTable::Impl& impl,
                                      ast::ASTNode* node, std::string libname) {
    SC_ASSERT(!impl.nativeLibMap.contains(libname),
              "This library is already imported");
    auto libpath =
        findNativeLibrary(impl.libSearchPaths,
                          utl::strcat(libname, ".", TargetNames::LibraryExt));
    if (!libpath) {
        impl.issue<BadImport>(node, BadImport::LibraryNotFound);
        return nullptr;
    }
    auto* lib =
        impl.addEntity<NativeLibrary>(libname, *libpath, &sym.globalScope());
    sym.globalScope().addChild(lib);
    /// We hide all libraries in the global scope. Scopes that want to use the
    /// library have to create an alias to it.
    lib->setVisible(false);
    impl.importedLibs.push_back(lib);
    impl.nativeLibMap.insert({ libname, lib });
    auto archive = Archive::Open(*libpath);
    SC_RELASSERT(archive, "Failed to open archive even though file exists");
    auto libsymtext = archive->openTextFile(TargetNames::SymbolTableName);
    SC_RELASSERT(libsymtext, "Failed to open library symbol table");
    sym.withScopeCurrent(lib, [&] {
        bool success = deserialize(sym, *libsymtext);
        SC_RELASSERT(success, "Failed to deserialize libary symbol table");
    });
    return lib;
}

NativeLibrary* SymbolTable::makeNativeLibAvailable(ast::Identifier& ID) {
    auto* lib = getOrImportNativeLib(ID.value(), &ID);
    if (!lib) {
        return nullptr;
    }
    declareAlias(std::string(ID.value()), *lib, &ID, AccessControl::Private);
    return lib;
}

NativeLibrary* SymbolTable::importNativeLib(std::string_view name) {
    return getOrImportNativeLib(name, nullptr);
}

ForeignLibrary* SymbolTable::importForeignLib(ast::Literal& lit) {
    SC_EXPECT(lit.kind() == ast::LiteralKind::String);
    return getOrImportForeignLib(lit.value<std::string>(), &lit);
}

ForeignLibrary* SymbolTable::importForeignLib(std::string_view libname) {
    return getOrImportForeignLib(libname, nullptr);
}

NativeLibrary* SymbolTable::getOrImportNativeLib(std::string_view libname,
                                                 ast::ASTNode* astNode) {
    auto itr = impl->nativeLibMap.find(libname);
    if (itr != impl->nativeLibMap.end()) {
        return itr->second;
    }
    return ::importNativeLib(*this, *impl, astNode, std::string(libname));
}

ForeignLibrary* SymbolTable::getOrImportForeignLib(std::string_view libname,
                                                   ast::ASTNode* astNode) {
    auto itr = impl->foreignLibMap.find(libname);
    if (itr != impl->foreignLibMap.end()) {
        return itr->second;
    }
    return ::importForeignLib(*this, *impl, astNode, std::string(libname));
}

StructType* SymbolTable::declareStructImpl(ast::StructDefinition* def,
                                           std::string name,
                                           AccessControl accessControl) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Other, name, def, accessControl)) {
        return nullptr;
    }
    auto* type = impl->addEntity<StructType>(name, &currentScope(), def,
                                             InvalidSize, InvalidSize,
                                             accessControl);
    impl->structTypes.push_back(type);
    addToCurrentScope(type);
    validateAccessControl(*type);
    addGlobalAliasIfInternalAtFilescope(type);
    return type;
}

StructType* SymbolTable::declareStructureType(ast::StructDefinition* def,
                                              AccessControl accessControl) {
    return declareStructImpl(def, std::string(def->name()), accessControl);
}

StructType* SymbolTable::declareStructureType(std::string name,
                                              AccessControl accessControl) {
    return declareStructImpl(nullptr, std::move(name), accessControl);
}

template <typename T, typename... Args>
T* SymbolTable::declareBuiltinType(Args&&... args) {
    SC_ASSERT(&currentScope() == &globalScope(),
              "Must use this function in global scope");
    auto* type =
        impl->addEntity<T>(std::forward<Args>(args)..., &currentScope());
    type->setBuiltin();
    addToCurrentScope(type);
    analyzeLifetime(*type, *this);
    return type;
}

Function* SymbolTable::declareFuncImpl(ast::FunctionDefinition* def,
                                       std::string name,
                                       AccessControl accessControl) {
    /// FIXME: This is a quick and dirty solution, we need to find a more
    /// general way to handle this
    if (isKeyword(name) && name != "move") {
        impl->issue<GenericBadStmt>(def, GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Function, name, def, accessControl)) {
        return nullptr;
    }
    Function* function =
        impl->addEntity<Function>(name, nullptr, &currentScope(),
                                  FunctionAttribute::None, def, accessControl);
    impl->functions.push_back(function);
    addToCurrentScope(function);
    addGlobalAliasIfInternalAtFilescope(function);
    return function;
}

Function* SymbolTable::declareFuncName(ast::FunctionDefinition* def,
                                       AccessControl accessControl) {
    return declareFuncImpl(def, std::string(def->name()), accessControl);
}

namespace {

enum class OSBuildErrorHandler { None, Fail };

} // namespace

/// Performs a DFS over the list of entities \p entities and returns the set of
/// all functions that are "referred to" by the entities.
///
/// Concretely this means the following:
/// All entities in the initial list are _considered_ based on their type:
/// - For aliases the aliased entity is _considered_
/// - Functions are added to the set if their signature has already been set
/// - For every other entity the error handler is invoked, which may invoke
///   undefined behaviour or do nothing
static utl::hashset<Function*> buildOverloadSet(
    std::span<Entity* const> entities, OSBuildErrorHandler errorHandler = {}) {
    struct Builder {
        std::span<Entity* const> entities;
        OSBuildErrorHandler errorHandler;
        utl::hashset<Function*> set;

        explicit Builder(std::span<Entity* const> entities,
                         OSBuildErrorHandler errorHandler):
            entities(entities), errorHandler(errorHandler) {}

        utl::hashset<Function*> build() {
            for (auto* entity: entities) {
                gather(*entity);
            }
            return std::move(set);
        }

        void gather(Entity& entity) {
            visit(entity, [this](auto& entity) { gatherImpl(entity); });
        }

        void gatherImpl(Alias& alias) { gather(*alias.aliased()); }

        void gatherImpl(Function& function) {
            if (function.type()) {
                set.insert(&function);
            }
        }

        void gatherImpl(Entity&) {
            using enum OSBuildErrorHandler;
            switch (errorHandler) {
            case None:
                break;
            case Fail:
                SC_UNREACHABLE();
            }
        }
    };
    return Builder(entities, errorHandler).build();
}

/// \Returns the set of functions with the same name as \p ref in the parent
/// scope of \p ref that already have a signature, not including \p ref
static utl::small_vector<Function*> findOtherOverloads(Entity* ref) {
    auto entities = ref->parent()->findEntities(ref->name());
    auto set = buildOverloadSet(entities, OSBuildErrorHandler::Fail);
    set.erase(stripAlias(ref));
    return set | ToSmallVector<>;
}

/// This function checks if \p entity which may be either a function or an alias
/// to a function is a valid overload with the other functions in its scope.
///
/// If it is not a valid overload an issue is pushed to the issue handler
///
/// \Returns `true` if \p entity is a valid overload
static bool checkValidOverload(SymbolTable::Impl& impl, Entity* entity,
                               std::span<Type const* const> argumentTypes) {
    SC_EXPECT(isa<Function>(stripAlias(entity)));
    auto overloadSet = findOtherOverloads(entity);
    auto* existing = findBySignature(std::span<Function* const>{ overloadSet },
                                     argumentTypes);
    if (existing) {
        impl.issue<Redefinition>(dyncast<ast::Declaration*>(entity->astNode()),
                                 existing);
        return false;
    }
    return true;
}

bool SymbolTable::setFunctionType(Function* function,
                                  std::span<Type const* const> argumentTypes,
                                  Type const* returnType) {
    return setFunctionType(function, functionType(argumentTypes, returnType));
}

bool SymbolTable::setFunctionType(Function* function,
                                  FunctionType const* type) {
    bool isReset = function->type() != nullptr;
    SC_ASSERT(isReset || function->type() == nullptr,
              "Function type has been set before");
    SC_ASSERT(!isReset || ranges::equal(function->argumentTypes(),
                                        type->argumentTypes()),
              "We may only reset function types to update the return type");
    function->_type = type;
    bool result = true;
    if (!isReset) {
        result &= checkValidOverload(*impl, function, type->argumentTypes());
        for (auto* alias: function->aliases()) {
            if (alias->name() == function->name()) {
                result &=
                    checkValidOverload(*impl, alias, type->argumentTypes());
            }
        }
    }
    if (function->returnType()) {
        validateAccessControl(*function);
    }
    return result;
}

Function* SymbolTable::declareFunction(std::string name,
                                       FunctionType const* type,
                                       AccessControl accessControl) {
    auto* function = declareFuncImpl(nullptr, std::move(name), accessControl);
    if (function && setFunctionType(function, type)) {
        return function;
    }
    return nullptr;
}

OverloadSet* SymbolTable::addOverloadSet(
    SourceRange sourceRange, utl::small_vector<Function*> functions) {
    SC_ASSERT(!functions.empty(), "The overload set cannot be empty");
    auto nameEq = [&](std::string_view name) {
        return functions.front()->name() == name;
    };
    SC_ASSERT(ranges::all_of(functions, nameEq, &Function::name),
              "All functions in an overload set must have the same name");
    return impl->addEntity<OverloadSet>(sourceRange, functions);
}

Function* SymbolTable::declareForeignFunction(std::string name,
                                              FunctionType const* type,
                                              FunctionAttribute attrs,
                                              AccessControl accessControl) {
    auto* function = declareFunction(name, type, accessControl);
    if (!function) {
        return nullptr;
    }
    function->setKind(FunctionKind::Foreign);
    function->setAttribute(attrs);
    if (auto builtinIndex = getBuiltinIndex(name)) {
        function->setBuiltin();
        if (impl->builtinFunctions.size() <= *builtinIndex) {
            impl->builtinFunctions.resize(*builtinIndex + 1);
        }
        impl->builtinFunctions[*builtinIndex] = function;
    }
    return function;
}

Variable* SymbolTable::declareVarImpl(ast::VarDeclBase* vardecl,
                                      std::string name,
                                      AccessControl accessControl,
                                      Mutability mut) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(vardecl,
                                    GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Other, name, vardecl, accessControl)) {
        return nullptr;
    }
    auto* var = impl->addEntity<Variable>(name, &currentScope(), vardecl,
                                          accessControl);
    var->setMutability(mut);
    addToCurrentScope(var);
    addGlobalAliasIfInternalAtFilescope(var);
    return var;
}

Variable* SymbolTable::defineVarImpl(ast::VarDeclBase* vardecl,
                                     std::string name, Type const* type,
                                     Mutability mut,
                                     AccessControl accessControl) {
    auto* var = declareVarImpl(vardecl, std::move(name), accessControl, mut);
    if (!var) {
        return nullptr;
    }
    setVariableType(var, type);
    return var;
}

Variable* SymbolTable::declareVariable(ast::VarDeclBase* vardecl,
                                       AccessControl accessControl) {
    return declareVarImpl(vardecl, std::string(vardecl->name()), accessControl,
                          vardecl->mutability());
}

bool SymbolTable::setVariableType(Variable* var, Type const* type) {
    var->_type = type;
    return validateAccessControl(*var);
}

Variable* SymbolTable::defineVariable(ast::VarDeclBase* vardecl,
                                      Type const* type, Mutability mut,
                                      AccessControl accessControl) {
    return defineVarImpl(vardecl, std::string(vardecl->name()), type, mut,
                         accessControl);
}

Variable* SymbolTable::defineVariable(std::string name, Type const* type,
                                      Mutability mut,
                                      AccessControl accessControl) {
    return defineVarImpl(nullptr, std::move(name), type, mut, accessControl);
}

Property* SymbolTable::addProperty(PropertyKind kind, Type const* type,
                                   Mutability mut, ValueCategory valueCat,
                                   AccessControl accessControl,
                                   ast::ASTNode* astNode) {
    auto* prop = impl->addEntity<Property>(kind, &currentScope(), type, mut,
                                           valueCat, accessControl, astNode);
    validateAccessControl(*prop);
    addToCurrentScope(prop);
    addGlobalAliasIfInternalAtFilescope(prop);
    return prop;
}

Temporary* SymbolTable::temporary(ast::ASTNode* astNode, QualType type) {
    auto* temp = impl->addEntity<Temporary>(impl->temporaryID++,
                                            &currentScope(), type, astNode);
    return temp;
}

Alias* SymbolTable::declareAlias(std::string name, Entity& aliased,
                                 ast::ASTNode* astNode,
                                 AccessControl accessControl) {
    auto existing = currentScope().findEntities(name);
    if (ranges::contains(existing, &aliased, stripAlias)) {
        return nullptr;
    }
    auto* alias = impl->addEntity<Alias>(std::move(name), aliased,
                                         &currentScope(), astNode,
                                         accessControl);
    addToCurrentScope(alias);
    aliased.addAlias(alias);
    return alias;
}

Alias* SymbolTable::declareAlias(Entity& aliased, ast::ASTNode* astNode,
                                 AccessControl accessControl) {
    return declareAlias(std::string(aliased.name()), aliased, astNode,
                        accessControl);
}

PoisonEntity* SymbolTable::declarePoison(ast::Identifier* ID,
                                         EntityCategory cat,
                                         AccessControl accessControl) {
    auto name = std::string(ID->value());
    if (isKeyword(name) || !currentScope().findEntities(name).empty()) {
        return nullptr;
    }
    auto* poison =
        impl->addEntity<PoisonEntity>(ID, cat, &currentScope(), accessControl);
    addToCurrentScope(poison);
    addGlobalAliasIfInternalAtFilescope(poison);
    return poison;
}

Scope* SymbolTable::addAnonymousScope() {
    auto* scope =
        impl->addEntity<AnonymousScope>(currentScope().kind(), &currentScope());
    addToCurrentScope(scope);
    return scope;
}

FunctionType const* SymbolTable::functionType(
    std::span<Type const* const> argumentTypes, Type const* returnType) {
    utl::small_vector<Type const*> argTypeVec = argumentTypes | ToSmallVector<>;
    FuncSig key = { argTypeVec, returnType };
    auto itr = impl->functionTypes.find(key);
    if (itr != impl->functionTypes.end()) {
        return itr->second;
    }
    auto* functionType =
        impl->addEntity<FunctionType>(std::move(argTypeVec), returnType);
    impl->functionTypes.insert({ key, functionType });
    return functionType;
}

FunctionType const* SymbolTable::functionType(
    std::initializer_list<Type const*> argumentTypes, Type const* returnType) {
    return functionType(std::span(argumentTypes), returnType);
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
    auto accessCtrl = arrayType->accessControl();
    withScopeCurrent(arrayType, [&] {
        using enum Mutability;
        using enum ValueCategory;
        auto* arraySize = addProperty(PropertyKind::ArraySize, S64(), Const,
                                      RValue, accessCtrl);
        if (size != ArrayType::DynamicCount) {
            auto constSize = allocate<IntValue>(APInt(size, 64),
                                                /* isSigned = */ true);
            arraySize->setConstantValue(std::move(constSize));
        }
        auto* arrayEmpty = addProperty(PropertyKind::ArrayEmpty, Bool(), Const,
                                       RValue, accessCtrl);
        if (size != ArrayType::DynamicCount) {
            auto constEmpty = allocate<IntValue>(APInt(size == 0, 1),
                                                 /* isSigned = */ false);
            arrayEmpty->setConstantValue(std::move(constEmpty));
        }
        addProperty(PropertyKind::ArrayFront, arrayType->elementType(), Mutable,
                    LValue, accessCtrl);
        addProperty(PropertyKind::ArrayBack, arrayType->elementType(), Mutable,
                    LValue, accessCtrl);
    });
    if (arrayType->elementType()->hasLifetimeMetadata()) {
        analyzeLifetime(*arrayType, *this);
    }
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
    return impl->ptrLikeImpl<RawPtrType>(impl->ptrTypes, pointee,
                                         [&](RawPtrType* type) {
        analyzeLifetime(*type, *this);
    });
}

RawPtrType const* SymbolTable::pointer(ObjectType const* type, Mutability mut) {
    return pointer(QualType(type, mut));
}

ReferenceType const* SymbolTable::reference(QualType referred) {
    return impl->ptrLikeImpl(impl->refTypes, referred);
}

ReferenceType const* SymbolTable::reference(ObjectType const* type,
                                            Mutability mut) {
    return reference(QualType(type, mut));
}

UniquePtrType const* SymbolTable::uniquePointer(QualType pointee) {
    return impl->ptrLikeImpl<UniquePtrType>(impl->uniquePtrTypes, pointee,
                                            [&](UniquePtrType* type) {
        analyzeLifetime(*type, *this);
    });
}

UniquePtrType const* SymbolTable::uniquePointer(ObjectType const* type,
                                                Mutability mut) {
    return uniquePointer(QualType(type, mut));
}

TypeDeductionQualifier* SymbolTable::typeDeductionQualifier(
    ReferenceKind refKind, Mutability mut) {
    auto& map = impl->typeDeductionQualifiers;
    auto itr = map.find({ refKind, mut });
    if (itr != map.end()) {
        return itr->second;
    }
    auto* qual = impl->addEntity<TypeDeductionQualifier>(refKind, mut);
    map.insert({ { refKind, mut }, qual });
    return qual;
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
    std::string_view name, bool findHiddenEntities) {
    utl::hashset<Entity*> overloadSet;
    for (auto* scope = &currentScope(); scope != nullptr;
         scope = scope->parent())
    {
        auto entities = scope->findEntities(name, findHiddenEntities);
        if (entities.empty()) {
            continue;
        }
        auto localOverloadSet = buildOverloadSet(entities);
        /// If we have functions we build up the overload set
        if (!localOverloadSet.empty()) {
            overloadSet.insert(localOverloadSet.begin(),
                               localOverloadSet.end());
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

NullPtrType const* SymbolTable::NullPtr() const { return impl->NullPtrType; }

std::span<Function* const> SymbolTable::functions() { return impl->functions; }

std::span<Function const* const> SymbolTable::functions() const {
    return impl->functions;
}

std::span<StructType const* const> SymbolTable::structTypes() const {
    return impl->structTypes;
}

std::span<Library* const> SymbolTable::importedLibs() {
    return impl->importedLibs;
}

std::span<Library const* const> SymbolTable::importedLibs() const {
    return impl->importedLibs;
}

std::vector<ForeignLibraryDecl> SymbolTable::foreignLibraries() const {
    return importedLibs() | Filter<ForeignLibrary> |
           transform([](ForeignLibrary const* lib) {
        return ForeignLibraryDecl(std::string(lib->name()), lib->file());
    }) | ranges::to<std::vector>;
}

std::vector<Entity const*> SymbolTable::entities() const {
    return impl->entities | ToConstAddress | ranges::to<std::vector>;
}

void SymbolTable::analyzeMissingLifetimes() {
    for (auto* type: impl->arrayTypes.values() | values) {
        if (!type->hasLifetimeMetadata() &&
            type->elementType()->hasLifetimeMetadata())
        {
            analyzeLifetime(const_cast<ArrayType&>(*type), *this);
        }
    }
}

static bool wantExport(Entity const& entity) {
    // clang-format off
    return SC_MATCH (entity) {
        [](Function const& F) { return F.isPublic() && !F.isBuiltin(); },
        [](StructType const& S) { return S.isPublic(); },
        [](Variable const& V) { return V.isPublic(); },
        [](ForeignLibrary const&) { return true; },
        [](Entity const&) { return false; }
    }; // clang-format on
}

static void prepareExportAlias(Alias& alias) {
    auto& aliased = *alias.aliased();
    auto* otherParent = aliased.parent();
    if (!wantExport(aliased) || otherParent == alias.parent() ||
        aliased.name() != alias.name())
    {
        return;
    }
    otherParent->removeChild(&aliased);
    alias.parent()->addChild(&aliased);
}

static void prepareExportRec(Scope& scope) {
    for (auto* entity: scope.entities() | ToSmallVector<>) {
        if (!wantExport(*entity) && !isa<BuiltinType>(entity)) {
            scope.removeChild(entity);
        }
        if (auto* scope = dyncast<Scope*>(entity)) {
            prepareExportRec(*scope);
        }
    }
}

void SymbolTable::prepareExport() {
    for (auto* alias:
         globalScope().entities() | Filter<Alias> | ToSmallVector<>)
    {
        prepareExportAlias(*alias);
    }
    prepareExportRec(globalScope());
}

template <typename E, typename... Args>
    requires std::constructible_from<E, Args...>
E* SymbolTable::Impl::addEntity(Args&&... args) {
    auto owner = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    entities.push_back(std::move(owner));
    return result;
}

void SymbolTable::addToCurrentScope(Entity* entity) {
    currentScope().addChild(entity);
}

Alias* SymbolTable::addGlobalAliasIfInternalAtFilescope(Entity* entity) {
    using enum AccessControl;
    if (!isa<FileScope>(entity->parent()) || entity->accessControl() > Internal)
    {
        return nullptr;
    }
    return withScopeCurrent(&globalScope(), [&] {
        return declareAlias(*entity, entity->astNode(),
                            entity->accessControl());
    });
}

bool SymbolTable::validateAccessControl(Entity const& entity) {
    SC_EXPECT(entity.hasAccessControl());
    auto* parent = entity.parent();
    if (parent->hasAccessControl() &&
        entity.accessControl() < parent->accessControl())
    {
        impl->issue<BadAccessControl>(&entity,
                                      BadAccessControl::TooWeakForParent);
        return false;
    }
    if (auto* type = getEntityType(entity);
        type && entity.accessControl() < type->accessControl())
    {
        impl->issue<BadAccessControl>(&entity,
                                      BadAccessControl::TooWeakForType);
        return false;
    }
    return true;
}

static bool checkRedefImpl(SymbolTable::Impl& impl, Redef kind,
                           Scope const* scope, std::string_view name,
                           ast::Declaration const* decl) {
    auto entities = scope->findEntities(name);
    switch (kind) {
    case Redef_Function: {
        auto mayOverload = [](Entity const* e) {
            e = stripAlias(e);
            SC_ASSERT(
                !isa<OverloadSet>(e),
                "Cannot be an overload set because overload sets are anonymous and cannot be found by name lookup");
            return isa<Function>(e);
        };
        if (ranges::all_of(entities, mayOverload)) {
            return true;
        }
        break;
    }
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
bool SymbolTable::checkRedef(int kind, std::string_view name,
                             ast::Declaration const* decl,
                             AccessControl accessControl) {
    if (!checkRedefImpl(*impl, Redef(kind), &currentScope(), name, decl)) {
        return false;
    }
    /// TODO: Rework this
    /// Since we now use aliases to declare entities to the global scope we may
    /// not need this anymore
    using enum AccessControl;
    if (isa<FileScope>(currentScope()) && accessControl <= Internal) {
        return checkRedefImpl(*impl, Redef(kind), &globalScope(), name, decl);
    }
    return true;
}
