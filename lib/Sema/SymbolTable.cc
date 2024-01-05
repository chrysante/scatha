#include "Sema/SymbolTable.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/function_view.hpp>
#include <utl/hashmap.hpp>
#include <utl/hashset.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/ForeignFunctionDecl.h"
#include "Common/Ranges.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/SemaIssues.h"
#include "Sema/Serialize.h"

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

    declareAlias("int", *impl->S64, nullptr, AccessControl::Public);
    declareAlias("float", *impl->F32, nullptr, AccessControl::Public);
    declareAlias("double", *impl->F64, nullptr, AccessControl::Public);
    declareAlias("str", *impl->Str, nullptr, AccessControl::Public);

    /// Declare builtin functions
#define SVM_BUILTIN_DEF(name, attrs, ...)                                      \
    declareForeignFunction("__builtin_" #name,                                 \
                           FunctionSignature(__VA_ARGS__),                     \
                           attrs,                                              \
                           AccessControl::Public);
    using enum FunctionAttribute;
#include <svm/Builtin.def>

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

static std::optional<std::filesystem::path> findLibrary(
    std::span<std::filesystem::path const> searchPaths, std::string name) {
    if (std::filesystem::exists(name)) {
        return std::filesystem::path(name);
    }
    for (auto& path: searchPaths) {
        auto fullPath = path / name;
        if (std::filesystem::exists(fullPath)) {
            return fullPath;
        }
    }
    return std::nullopt;
}

static std::string toForeignLibName(std::string_view name) {
    /// TODO: Make portable
    /// This is the MacOS convention, need to add linux and windows conventions
    /// for portability
    return utl::strcat("lib", name, ".dylib");
}

static ForeignLibrary* importForeignLib(SymbolTable& sym,
                                        SymbolTable::Impl& impl,
                                        ast::ASTNode* astNode,
                                        std::string libname) {
    SC_ASSERT(!impl.foreignLibMap.contains(libname),
              "This library is already imported");
    auto path = findLibrary(impl.libSearchPaths, toForeignLibName(libname));
    if (!path) {
        if (astNode) {
            impl.issue<BadImport>(astNode, BadImport::LibraryNotFound);
        }
        else {
            impl.issue<BadImport>(libname);
        }
        return nullptr;
    }
    auto* lib = impl.addEntity<ForeignLibrary>(std::string{},
                                               libname,
                                               *path,
                                               &sym.globalScope());
    /// We add foreign libraries to the global scope because this makes
    /// exporting foreign library imports from native libraries easier. This
    /// does not affect name lookup because foreign libs don't expose names in
    /// the frontend
    sym.globalScope().addChild(lib);
    impl.importedLibs.push_back(lib);
    impl.foreignLibMap.insert({ libname, lib });
    return lib;
}

static std::filesystem::path replaceExt(std::filesystem::path p,
                                        std::filesystem::path ext) {
    p.replace_extension(ext);
    return p;
}

static NativeLibrary* importNativeLib(SymbolTable& sym,
                                      SymbolTable::Impl& impl,
                                      ast::ASTNode* node,
                                      std::string libname) {
    SC_ASSERT(!impl.nativeLibMap.contains(libname),
              "This library is already imported");
    auto symPath =
        findLibrary(impl.libSearchPaths, utl::strcat(libname, ".scsym"));
    if (!symPath) {
        impl.issue<BadImport>(node, BadImport::LibraryNotFound);
        return nullptr;
    }
    std::filesystem::path irPath = replaceExt(*symPath, "scir");
    if (!std::filesystem::exists(irPath)) {
        impl.issue<BadImport>(node, BadImport::LibraryNotFound);
        return nullptr;
    }
    /// We declare the library without parent scope. Scopes that want to use the
    /// library have to create an alias to it.
    auto* lib = impl.addEntity<NativeLibrary>(std::string{},
                                              libname,
                                              irPath,
                                              &sym.globalScope());
    sym.globalScope().addChild(lib);
    impl.importedLibs.push_back(lib);
    impl.nativeLibMap.insert({ libname, lib });
    std::fstream symFile(*symPath);
    SC_RELASSERT(symFile,
                 utl::strcat("Failed to open file ",
                             *symPath,
                             " even though it exists")
                     .c_str());
    sym.withScopeCurrent(lib, [&] {
        bool success = deserialize(sym, symFile);
        SC_RELASSERT(success,
                     utl::strcat("Failed to deserialize ", *symPath).c_str());
    });
    return lib;
}

NativeLibrary* SymbolTable::makeNativeLibAvailable(ast::Identifier& ID) {
    auto* lib = getOrImportNativeLib(ID.value(), &ID);
    if (lib) {
        declareAlias(std::string(ID.value()),
                     *lib,
                     &ID,
                     AccessControl::Private);
    }
    return lib;
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
    return importNativeLib(*this, *impl, astNode, std::string(libname));
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
    if (!checkRedef(Redef_Other, name, def)) {
        return nullptr;
    }
    auto* type = impl->addEntity<StructType>(name,
                                             &currentScope(),
                                             def,
                                             InvalidSize,
                                             InvalidSize,
                                             accessControl);
    addToCurrentScope(type);
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
    if (!checkRedef(Redef_Function, name, def)) {
        return nullptr;
    }
    Function* function = impl->addEntity<Function>(name,
                                                   &currentScope(),
                                                   FunctionAttribute::None,
                                                   def,
                                                   accessControl);
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
/// - For overload sets all functions in the set are _considered_
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
            if (function.hasSignature()) {
                set.insert(&function);
            }
        }

        void gatherImpl(OverloadSet& os) {
            for (auto* function: os) {
                gather(*function);
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
static utl::small_vector<Entity*> findOtherOverloads(Entity* ref) {
    auto entities = ref->parent()->findEntities(ref->name());
    auto set = buildOverloadSet(entities, OSBuildErrorHandler::Fail);
    set.erase(ref);
    set.erase(stripAlias(ref));
    return set | transform(cast<Entity*>) | ToSmallVector<>;
}

/// This function checks if \p entity which may be either a function or an alias
/// to a function is a valid overload with the other functions in its scope.
///
/// If it is not a valid overload an issue is pushed to the issue handler
///
/// \Returns `true` if \p entity is a valid overload
static bool checkValidOverload(SymbolTable::Impl& impl,
                               Entity* entity,
                               std::span<Type const* const> argumentTypes) {
    SC_EXPECT(isa<Function>(stripAlias(entity)));
    auto overloadSet = findOtherOverloads(entity);
    auto* existing = OverloadSet::find(std::span<Entity* const>{ overloadSet },
                                       argumentTypes);
    if (existing) {
        impl.issue<Redefinition>(dyncast<ast::Declaration*>(entity->astNode()),
                                 existing);
        return false;
    }
    return true;
}

bool SymbolTable::setFuncSig(Function* function, FunctionSignature sig) {
    function->setSignature(sig); /// We don't move `sig` here because we use it
                                 /// later in this function
    bool result = true;
    result &= checkValidOverload(*impl, function, sig.argumentTypes());
    for (auto* alias: function->aliases()) {
        if (alias->name() == function->name()) {
            result &= checkValidOverload(*impl, alias, sig.argumentTypes());
        }
    }
    return result;
}

Function* SymbolTable::declareFunction(std::string name,
                                       FunctionSignature sig,
                                       AccessControl accessControl) {
    auto* function = declareFuncImpl(nullptr, std::move(name), accessControl);
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
                                              FunctionSignature sig,
                                              FunctionAttribute attrs,
                                              AccessControl accessControl) {
    auto* function = declareFunction(name, std::move(sig), accessControl);
    if (!function) {
        return nullptr;
    }
    function->setForeign();
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
                                      AccessControl accessControl) {
    if (isKeyword(name)) {
        impl->issue<GenericBadStmt>(vardecl,
                                    GenericBadStmt::ReservedIdentifier);
        return nullptr;
    }
    if (!checkRedef(Redef_Other, name, vardecl)) {
        return nullptr;
    }
    auto* variable = impl->addEntity<Variable>(name,
                                               &currentScope(),
                                               vardecl,
                                               accessControl);
    addToCurrentScope(variable);
    addGlobalAliasIfInternalAtFilescope(variable);
    return variable;
}

Variable* SymbolTable::defineVarImpl(ast::VarDeclBase* vardecl,
                                     std::string name,
                                     Type const* type,
                                     Mutability mut,
                                     AccessControl accessControl) {
    auto* var = declareVarImpl(vardecl, std::move(name), accessControl);
    if (!var) {
        return nullptr;
    }
    var->setType(type);
    var->setMutability(mut);
    return var;
}

Variable* SymbolTable::declareVariable(ast::VarDeclBase* vardecl,
                                       AccessControl accessControl) {
    return declareVarImpl(vardecl, std::string(vardecl->name()), accessControl);
}

Variable* SymbolTable::declareVariable(std::string name,
                                       AccessControl accessControl) {
    return declareVarImpl(nullptr, std::move(name), accessControl);
}

Variable* SymbolTable::defineVariable(ast::VarDeclBase* vardecl,
                                      Type const* type,
                                      Mutability mut,
                                      AccessControl accessControl) {
    return defineVarImpl(vardecl,
                         std::string(vardecl->name()),
                         type,
                         mut,
                         accessControl);
}

Variable* SymbolTable::defineVariable(std::string name,
                                      Type const* type,
                                      Mutability mut,
                                      AccessControl accessControl) {
    return defineVarImpl(nullptr, std::move(name), type, mut, accessControl);
}

Property* SymbolTable::addProperty(PropertyKind kind,
                                   Type const* type,
                                   Mutability mut,
                                   ValueCategory valueCat,
                                   AccessControl accessControl) {
    auto* prop = impl->addEntity<Property>(kind,
                                           &currentScope(),
                                           type,
                                           mut,
                                           valueCat,
                                           accessControl);
    addToCurrentScope(prop);
    addGlobalAliasIfInternalAtFilescope(prop);
    return prop;
}

Temporary* SymbolTable::temporary(QualType type) {
    auto* temp =
        impl->addEntity<Temporary>(impl->temporaryID++, &currentScope(), type);
    return temp;
}

Alias* SymbolTable::declareAlias(std::string name,
                                 Entity& aliased,
                                 ast::ASTNode* astNode,
                                 AccessControl accessControl) {
    std::span existing = currentScope().findEntities(name);
    if (ranges::contains(existing, &aliased, stripAlias)) {
        return nullptr;
    }
    auto* alias = impl->addEntity<Alias>(std::move(name),
                                         aliased,
                                         &currentScope(),
                                         astNode,
                                         accessControl);
    addToCurrentScope(alias);
    aliased.addAlias(alias);
    return alias;
}

Alias* SymbolTable::declareAlias(Entity& aliased,
                                 ast::ASTNode* astNode,
                                 AccessControl accessControl) {
    return declareAlias(std::string(aliased.name()),
                        aliased,
                        astNode,
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
        auto* arraySize = addProperty(PropertyKind::ArraySize,
                                      S64(),
                                      Const,
                                      RValue,
                                      AccessControl::Public);
        if (size != ArrayType::DynamicCount) {
            auto constSize = allocate<IntValue>(APInt(size, 64),
                                                /* isSigned = */ true);
            arraySize->setConstantValue(std::move(constSize));
        }
        auto* arrayEmpty = addProperty(PropertyKind::ArrayEmpty,
                                       Bool(),
                                       Const,
                                       RValue,
                                       AccessControl::Public);
        if (size != ArrayType::DynamicCount) {
            auto constEmpty = allocate<IntValue>(APInt(size == 0, 1),
                                                 /* isSigned = */ false);
            arrayEmpty->setConstantValue(std::move(constEmpty));
        }
        addProperty(PropertyKind::ArrayFront,
                    arrayType->elementType(),
                    Mutable,
                    LValue,
                    AccessControl::Public);
        addProperty(PropertyKind::ArrayBack,
                    arrayType->elementType(),
                    Mutable,
                    LValue,
                    AccessControl::Public);
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

std::span<Library* const> SymbolTable::importedLibs() {
    return impl->importedLibs;
}

std::span<Library const* const> SymbolTable::importedLibs() const {
    return impl->importedLibs;
}

std::vector<std::filesystem::path> SymbolTable::foreignLibraryPaths() const {
    return importedLibs() | Filter<ForeignLibrary> |
           transform(&ForeignLibrary::file) | ranges::to<std::vector>;
}

std::vector<std::string> SymbolTable::foreignLibraryNames() const {
    return importedLibs() | Filter<ForeignLibrary> |
           transform([](auto* lib) { return std::string(lib->name()); }) |
           ranges::to<std::vector>;
}

std::vector<Entity const*> SymbolTable::entities() const {
    return impl->entities | ToConstAddress | ranges::to<std::vector>;
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

void SymbolTable::addGlobalAliasIfInternalAtFilescope(Entity* entity) {
    using enum AccessControl;
    if (isa<FileScope>(entity->parent()) && entity->accessControl() >= Internal)
    {
        withScopeCurrent(&globalScope(), [&] {
            declareAlias(*entity, entity->astNode(), entity->accessControl());
        });
    }
}

static bool checkRedefImpl(SymbolTable::Impl& impl,
                           Redef kind,
                           Scope const* scope,
                           std::string_view name,
                           ast::Declaration const* decl) {
    auto entities = scope->findEntities(name);
    switch (kind) {
    case Redef_Function: {
        auto mayOverload = [](Entity const* e) {
            e = stripAlias(e);
            return isa<Function>(e) || isa<OverloadSet>(e);
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
bool SymbolTable::checkRedef(int kind,
                             std::string_view name,
                             ast::Declaration const* decl) {
    if (!checkRedefImpl(*impl, Redef(kind), &currentScope(), name, decl)) {
        return false;
    }
    /// TODO: Rework this
    /// Since we now use aliases to declare entities to the global scope we may
    /// not need this anymore
    using enum AccessControl;
    if (isa<FileScope>(currentScope()) &&
        decl->accessControl().value() >= Internal)
    {
        return checkRedefImpl(*impl, Redef(kind), &globalScope(), name, decl);
    }
    return true;
}
