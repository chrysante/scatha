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

    /// Declare `void` with `InvalidSize` to make it an incomplete type.
    _void = cast<StructureType*>(
        declareBuiltinType("void", InvalidSize, InvalidSize));
    _byte   = cast<StructureType*>(declareBuiltinType("byte", 1, 1));
    _bool   = cast<StructureType*>(declareBuiltinType("bool", 1, 1));
    _s8     = cast<StructureType*>(declareBuiltinType("s8", 1, 1));
    _s16    = cast<StructureType*>(declareBuiltinType("s16", 2, 2));
    _s32    = cast<StructureType*>(declareBuiltinType("s32", 4, 4));
    _s64    = cast<StructureType*>(declareBuiltinType("s64", 8, 8));
    _u8     = cast<StructureType*>(declareBuiltinType("u8", 1, 1));
    _u16    = cast<StructureType*>(declareBuiltinType("u16", 2, 2));
    _u32    = cast<StructureType*>(declareBuiltinType("u32", 4, 4));
    _u64    = cast<StructureType*>(declareBuiltinType("u64", 8, 8));
    _float  = cast<StructureType*>(declareBuiltinType("float", 8, 8));
    _string = cast<StructureType*>(declareBuiltinType("string",
                                                      sizeof(std::string),
                                                      alignof(std::string)));

    _s64->addAlternateName("int");

    /// Declare builtin functions
    _builtinFunctions.resize(static_cast<size_t>(svm::Builtin::_count));
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

Type* SymbolTable::declareBuiltinType(std::string name,
                                      size_t size,
                                      size_t align) {
    auto result = declareStructureType(name, /* allowKeywords = */ true);
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

bool SymbolTable::declareSpecialFunction(FunctionKind kind,
                                         std::string name,
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
    function._kind  = kind;
    function.attrs  = attrs;
    function._slot  = utl::narrow_cast<u16>(slot);
    function._index = utl::narrow_cast<u32>(index);
    if (kind == FunctionKind::External && slot == svm::BuiltinFunctionSlot) {
        _builtinFunctions[index] = &function;
    }
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

ArrayType const* SymbolTable::arrayType(ObjectType const* elementType,
                                        size_t size) {
    std::pair key = { elementType, size };
    auto itr      = _arrayTypes.find(key);
    if (itr != _arrayTypes.end()) {
        return itr->second;
    }
    auto* arrayType = addEntity<ArrayType>(elementType, size);
    _arrayTypes.insert({ key, arrayType });
    withScopeCurrent(arrayType, [&] {
        auto* countVar = &addVariable("count", qS64()).value();
        countVar->setIndex(1);
        arrayType->setCountVariable(countVar);
    });
    return arrayType;
}

QualType const* SymbolTable::qualify(Type const* base,
                                     TypeQualifiers qualifiers) {
    ObjectType const* objType = dyncast<ObjectType const*>(base);
    if (!objType) {
        objType = cast<QualType const*>(base)->base();
    }
    return getQualType(objType, qualifiers);
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

QualType const* SymbolTable::arrayView(ObjectType const* base,
                                       TypeQualifiers qualifiers) {
    return qualify(arrayType(base, ArrayType::DynamicCount),
                   ImplicitReference | qualifiers);
}

QualType const* SymbolTable::getQualType(ObjectType const* baseType,
                                         TypeQualifiers qualifiers) {
    std::pair key = { baseType, qualifiers };
    auto itr      = _qualTypes.find(key);
    if (itr != _qualTypes.end()) {
        return itr->second;
    }
    auto* qualType =
        addEntity<QualType>(const_cast<ObjectType*>(baseType), qualifiers);
    _qualTypes.insert({ key, qualType });
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

QualType const* SymbolTable::qVoid(TypeQualifiers qualifiers) {
    return qualify(_void, qualifiers);
}

QualType const* SymbolTable::qByte(TypeQualifiers qualifiers) {
    return qualify(_bool, qualifiers);
}

QualType const* SymbolTable::qBool(TypeQualifiers qualifiers) {
    return qualify(_bool, qualifiers);
}

QualType const* SymbolTable::qS8(TypeQualifiers qualifiers) {
    return qualify(_s8, qualifiers);
}

QualType const* SymbolTable::qS16(TypeQualifiers qualifiers) {
    return qualify(_s16, qualifiers);
}

QualType const* SymbolTable::qS32(TypeQualifiers qualifiers) {
    return qualify(_s32, qualifiers);
}

QualType const* SymbolTable::qS64(TypeQualifiers qualifiers) {
    return qualify(_s64, qualifiers);
}

QualType const* SymbolTable::qU8(TypeQualifiers qualifiers) {
    return qualify(_u8, qualifiers);
}

QualType const* SymbolTable::qU16(TypeQualifiers qualifiers) {
    return qualify(_u16, qualifiers);
}

QualType const* SymbolTable::qU32(TypeQualifiers qualifiers) {
    return qualify(_u32, qualifiers);
}

QualType const* SymbolTable::qU64(TypeQualifiers qualifiers) {
    return qualify(_u64, qualifiers);
}

QualType const* SymbolTable::qFloat(TypeQualifiers qualifiers) {
    return qualify(_float, qualifiers);
}

QualType const* SymbolTable::qString(TypeQualifiers qualifiers) {
    return qualify(_string, qualifiers);
}

template <typename E, typename... Args>
E* SymbolTable::addEntity(Args&&... args) {
    auto owner   = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    _entities.push_back(std::move(owner));
    return result;
}
