#include "Sema/SymbolTable.h"

#include <string>

#include <svm/Builtin.h>
#include <utl/utility.hpp>

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

SymbolTable::SymbolTable() {
    auto globalScopeID  = generateID(SymbolCategory::Invalid);
    auto [itr, success] = _entities.insert(
        { globalScopeID, allocate<GlobalScope>(globalScopeID) });
    _globalScope  = cast<GlobalScope*>(itr->second.get());
    _currentScope = _globalScope;

    /// Declare `void` with `invalidSize` to make it an incomplete type.
    _void  = declareBuiltinType("void", invalidSize, invalidSize);
    _bool  = declareBuiltinType("bool", 1, 1);
    _int   = declareBuiltinType("int", 8, 8);
    _float = declareBuiltinType("float", 8, 8);
    _string =
        declareBuiltinType("string", sizeof(std::string), alignof(std::string));

    /// Declare builtin functions
    _builtinFunctions.resize(static_cast<size_t>(svm::Builtin::_count));
#define SVM_BUILTIN_DEF(name, attrs, ...)                                      \
    declareExternalFunction(                                                   \
        "__builtin_" #name,                                                    \
        /* slot = */ svm::BuiltinFunctionSlot,                                 \
        /* index = */ static_cast<size_t>(svm::Builtin::name),                 \
        FunctionSignature(__VA_ARGS__),                                        \
        attrs);
    using enum FunctionAttribute;
#include <svm/Builtin.def>
}

SymbolTable::SymbolTable(SymbolTable&&) noexcept = default;

SymbolTable& SymbolTable::operator=(SymbolTable&&) noexcept = default;

SymbolTable::~SymbolTable() = default;

Expected<ObjectType&, SemanticIssue*> SymbolTable::declareObjectType(
    std::string name, bool allowKeywords) {
    using enum InvalidDeclaration::Reason;
    if (!allowKeywords && isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope(),
                                      SymbolCategory::Type);
    }
    const SymbolID symbolID = currentScope().findID(name);
    if (symbolID != SymbolID::Invalid) {
        return new InvalidDeclaration(nullptr,
                                      Redefinition,
                                      currentScope(),
                                      SymbolCategory::Type,
                                      symbolID.category());
    }
    auto const newSymbolID = generateID(SymbolCategory::Type);
    auto [itr, success]    = _entities.insert(
        { newSymbolID,
             allocate<ObjectType>(name, newSymbolID, &currentScope()) });
    SC_ASSERT(success, "");
    currentScope().add(itr->second.get());
    return cast<ObjectType&>(*itr->second);
}

Type const* SymbolTable::declareBuiltinType(std::string name,
                                            size_t size,
                                            size_t align) {
    auto result = declareObjectType(name, /* allowKeywords = */ true);
    SC_ASSERT(result, "How could this fail?");
    result->setSize(size);
    result->setAlign(align);
    result->setIsBuiltin(true);
    return &result.value();
}

Expected<Function const&, SemanticIssue*> SymbolTable::declareFunction(
    std::string name) {
    using enum InvalidDeclaration::Reason;
    if (isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope(),
                                      SymbolCategory::Function);
    }
    SymbolID const overloadSetID = [&] {
        auto const symbolID = currentScope().findID(name);
        if (symbolID != SymbolID::Invalid) {
            return symbolID;
        }
        /// Create a new overload set
        auto const newSymbolID = generateID(SymbolCategory::OverloadSet);
        auto [itr, success]    = _entities.insert(
            { newSymbolID,
                 allocate<OverloadSet>(name, newSymbolID, &currentScope()) });
        SC_ASSERT(success, "");
        OverloadSet& overloadSet = cast<OverloadSet&>(*itr->second);
        currentScope().add(&overloadSet);
        return overloadSet.symbolID();
    }();
    /// Even if we get a valid ID from the lambda, we don't know yet if it's
    /// really an overload set.
    auto* const overloadSetPtr = tryGet<OverloadSet>(overloadSetID);
    if (!overloadSetPtr) {
        return new InvalidDeclaration(nullptr,
                                      InvalidDeclaration::Reason::Redefinition,
                                      currentScope(),
                                      SymbolCategory::Function,
                                      overloadSetID.category());
    }
    auto const newSymbolID    = generateID(SymbolCategory::Function);
    auto const [itr, success] = _entities.insert(
        { newSymbolID,
          allocate<Function>(name,
                             /* functionID = */ newSymbolID,
                             /* overloadSetID = */ overloadSetID,
                             &currentScope(),
                             FunctionAttribute::None) });
    SC_ASSERT(success, "?");
    Function& function = cast<Function&>(*itr->second);
    currentScope().add(&function);
    return function;
}

Expected<void, SemanticIssue*> SymbolTable::setSignature(
    SymbolID functionID, FunctionSignature sig) {
    auto& function                      = get<Function>(functionID);
    auto const overloadSetID            = function.overloadSetID();
    auto& overloadSet                   = get<OverloadSet>(overloadSetID);
    function._sig                       = std::move(sig);
    auto const [otherFunction, success] = overloadSet.add(&function);
    if (!success) {
        using enum InvalidDeclaration::Reason;
        InvalidDeclaration::Reason const reason =
            otherFunction->signature().returnType() ==
                    function.signature().returnType() ?
                Redefinition :
                CantOverloadOnReturnType;
        return new InvalidDeclaration(nullptr,
                                      reason,
                                      currentScope(),
                                      SymbolCategory::Function);
    }
    return {};
}

bool SymbolTable::declareExternalFunction(std::string name,
                                          size_t slot,
                                          size_t index,
                                          FunctionSignature signature,
                                          FunctionAttribute attrs) {
    utl::scope_guard restoreScope = [this, scope = &currentScope()] {
        makeScopeCurrent(scope);
    };
    makeScopeCurrent(nullptr);
    auto declResult = declareFunction(std::move(name));
    if (!declResult) {
        return false;
    }
    auto& decl = const_cast<Function&>(*declResult);
    setSignature(decl.symbolID(), std::move(signature));
    decl._isExtern           = true;
    decl._slot               = utl::narrow_cast<u32>(slot);
    decl._index              = utl::narrow_cast<u32>(index);
    decl.attrs               = attrs;
    _builtinFunctions[index] = decl.symbolID();
    return true;
}

Expected<Variable&, SemanticIssue*> SymbolTable::declareVariable(
    std::string name) {
    using enum InvalidDeclaration::Reason;
    if (isKeyword(name)) {
        return new InvalidDeclaration(nullptr,
                                      ReservedIdentifier,
                                      currentScope(),
                                      SymbolCategory::Variable);
    }
    const SymbolID symbolID = currentScope().findID(name);
    if (symbolID != SymbolID::Invalid) {
        return new InvalidDeclaration(nullptr,
                                      Redefinition,
                                      currentScope(),
                                      SymbolCategory::Variable,
                                      symbolID.category());
    }
    auto const newSymbolID = generateID(SymbolCategory::Variable);
    auto [itr, success]    = _entities.insert(
        { newSymbolID,
             allocate<Variable>(name, newSymbolID, &currentScope()) });
    SC_ASSERT(success, "");
    Variable& variable = cast<Variable&>(*itr->second);
    currentScope().add(&variable);
    return variable;
}

Expected<Variable&, SemanticIssue*> SymbolTable::addVariable(std::string name,
                                                             Type const* type,
                                                             size_t offset) {
    auto declResult = declareVariable(std::move(name));
    if (!declResult) {
        return declResult.error();
    }
    auto& var = *declResult;
    var.setType(type);
    SC_ASSERT(offset == 0, "Temporary measure. Should remove parameter offset");
    return var;
}

Scope const& SymbolTable::addAnonymousScope() {
    auto const symbolID = generateID(SymbolCategory::Anonymous);
    auto [itr, success] =
        _entities.insert({ symbolID,
                           allocate<AnonymousScope>(symbolID,
                                                    currentScope().kind(),
                                                    &currentScope()) });
    SC_ASSERT(success, "");
    Scope& scope = cast<Scope&>(*itr->second);
    currentScope().add(&scope);
    return scope;
}

QualType const* SymbolTable::qualifyType(Type const* base,
                                         TypeQualifiers qualifiers) {
    ObjectType const* objType = dyncast<ObjectType const*>(base);
    if (!objType) {
        objType = cast<QualType const*>(base)->base();
    }
    return getQualType(objType, qualifiers);
}

QualType const* SymbolTable::getQualType(ObjectType const* baseType,
                                      TypeQualifiers qualifiers) {
    if (auto itr = _qualTypes.find({ baseType, qualifiers });
        itr != _qualTypes.end())
    {
        return itr->second;
    }
    auto newSymbolID = generateID(SymbolCategory::Type);
    auto [itr, success] =
        _entities.insert({ newSymbolID,
                           allocate<QualType>(newSymbolID,
                                              const_cast<ObjectType*>(baseType),
                                              qualifiers) });
    SC_ASSERT(success, "");
    return cast<QualType*>(itr->second.get());
}

void SymbolTable::pushScope(SymbolID id) {
    auto const itr = currentScope()._children.find(id);
    SC_ASSERT(itr != currentScope()._children.end(), "not found");
    _currentScope = itr->second;
}

void SymbolTable::popScope() { _currentScope = currentScope().parent(); }

void SymbolTable::makeScopeCurrent(Scope* scope) {
    _currentScope = scope ? scope : &globalScope();
}

Entity const& SymbolTable::get(SymbolID id) const {
    auto const* result = tryGet(id);
    SC_ASSERT(result, "ID must be valid");
    return *result;
}

Entity const* SymbolTable::tryGet(SymbolID id) const {
    auto const itr = _entities.find(id);
    if (itr == _entities.end()) {
        return nullptr;
    }
    return itr->second.get();
}

SymbolID SymbolTable::lookup(std::string_view name) const {
    Scope const* scope = &currentScope();
    while (scope != nullptr) {
        auto const id = scope->findID(name);
        if (id != SymbolID::Invalid) {
            return id;
        }
        scope = scope->parent();
    }
    return SymbolID::Invalid;
}

SymbolID SymbolTable::generateID(SymbolCategory cat) {
    return SymbolID(_idCounter++, cat);
}
