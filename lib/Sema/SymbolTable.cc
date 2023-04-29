#include "Sema/SymbolTable.h"

#include <string>

#include <svm/Builtin.h>
#include <utl/utility.hpp>

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

SymbolTable::SymbolTable() {
    _currentScope = _globalScope = addEntity<GlobalScope>();

    /// Declare `void` with `invalidSize` to make it an incomplete type.
    _void = declareBuiltinType("void", invalidSize, invalidSize);
    _byte = declareBuiltinType("byte", 1, 1);
    _bool = declareBuiltinType("bool", 1, 1);
#if 0
    _s8    = declareBuiltinType("s8",  1, 1);
    _s16   = declareBuiltinType("s16", 2, 2);
    _s32   = declareBuiltinType("s32", 4, 4);
    _s64   = declareBuiltinType("s64", 8, 8);
    _u8    = declareBuiltinType("u8",  1, 1);
    _u16   = declareBuiltinType("u16", 2, 2);
    _u32   = declareBuiltinType("u32", 4, 4);
    _u64   = declareBuiltinType("u64", 8, 8);
    _f32   = declareBuiltinType("f32", 4, 4);
    _f64   = declareBuiltinType("f64", 8, 8);
#endif
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
                                      currentScope());
    }
    if (Entity* entity = currentScope().findEntity(name)) {
        return new InvalidDeclaration(nullptr, Redefinition, currentScope());
    }
    auto* type = addEntity<ObjectType>(name, &currentScope());
    currentScope().add(type);
    return *type;
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
    _functions.push_back(function);
    return *function;
}

Expected<void, SemanticIssue*> SymbolTable::setSignature(
    Function* function, FunctionSignature sig) {
    auto* overloadSet                   = function->overloadSet();
    function->_sig                      = std::move(sig);
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
    auto& function = *declResult;
    setSignature(&function, std::move(signature));
    function._isExtern = true;
    function._slot     = utl::narrow_cast<u32>(slot);
    function._index    = utl::narrow_cast<u32>(index);
    function.attrs     = attrs;
    // FIXME: Make sure only builtin function are added here
    _builtinFunctions[index] = &function;
    return true;
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
    std::string name, QualType const* type, size_t offset) {
    auto declResult = declareVariable(std::move(name));
    if (!declResult) {
        return declResult.error();
    }
    auto& var = *declResult;
    var.setType(type);
    SC_ASSERT(offset == 0, "Temporary measure. Should remove parameter offset");
    return var;
}

Scope& SymbolTable::addAnonymousScope() {
    auto* scope =
        addEntity<AnonymousScope>(currentScope().kind(), &currentScope());
    currentScope().add(scope);
    return *scope;
}

QualType const* SymbolTable::qualify(Type const* base,
                                     TypeQualifiers qualifiers,
                                     size_t arraySize) {
    ObjectType const* objType = dyncast<ObjectType const*>(base);
    if (!objType) {
        objType = cast<QualType const*>(base)->base();
    }
    return getQualType(objType, qualifiers, arraySize);
}

QualType const* SymbolTable::addQualifiers(QualType const* base,
                                           TypeQualifiers qualifiers) {
    SC_ASSERT(base, "");
    return qualify(base, base->qualifiers() | qualifiers);
}

QualType const* SymbolTable::removeQualifiers(QualType const* base,
                                              TypeQualifiers qualifiers) {
    return qualify(base, base->qualifiers() & ~qualifiers);
}

QualType const* SymbolTable::arrayView(Type const* base,
                                       TypeQualifiers qualifiers) {
    SC_ASSERT(base, "");
    using enum TypeQualifiers;
    return qualify(base, Array | ImplicitReference | qualifiers);
}

QualType const* SymbolTable::getQualType(ObjectType const* baseType,
                                         TypeQualifiers qualifiers,
                                         size_t arraySize) {
    if (auto itr = _qualTypes.find({ baseType, qualifiers, arraySize });
        itr != _qualTypes.end())
    {
        return itr->second;
    }
    auto* qualType = addEntity<QualType>(const_cast<ObjectType*>(baseType),
                                         qualifiers,
                                         arraySize);
    _qualTypes.insert(
        { std::tuple{ baseType, qualifiers, arraySize }, qualType });
    return qualType;
}

void SymbolTable::pushScope(Scope* scope) {
    SC_ASSERT(currentScope().isChildScope(scope),
              "Scope must be a child of the current scope");
    _currentScope = scope;
}

void SymbolTable::popScope() { _currentScope = currentScope().parent(); }

void SymbolTable::makeScopeCurrent(Scope* scope) {
    _currentScope = scope ? scope : &globalScope();
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

template <typename E, typename... Args>
E* SymbolTable::addEntity(Args&&... args) {
    auto owner   = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    _entities.push_back(std::move(owner));
    return result;
}
