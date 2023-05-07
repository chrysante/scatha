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

    _void  = declareBuiltinType<VoidType>();
    _byte  = declareBuiltinType<ByteType>();
    _bool  = declareBuiltinType<BoolType>();
    _s8    = declareBuiltinType<IntType>(8, IntType::Signed);
    _s16   = declareBuiltinType<IntType>(16, IntType::Signed);
    _s32   = declareBuiltinType<IntType>(32, IntType::Signed);
    _s64   = declareBuiltinType<IntType>(64, IntType::Signed);
    _u8    = declareBuiltinType<IntType>(8, IntType::Unsigned);
    _u16   = declareBuiltinType<IntType>(16, IntType::Unsigned);
    _u32   = declareBuiltinType<IntType>(32, IntType::Unsigned);
    _u64   = declareBuiltinType<IntType>(64, IntType::Unsigned);
    _float = declareBuiltinType<FloatType>(64);

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
    _functions.push_back(function);
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

QualType const* SymbolTable::qualify(ObjectType const* base, Reference ref) {
    std::pair key = { base, ref };
    auto itr      = _qualTypes.find(key);
    if (itr != _qualTypes.end()) {
        return itr->second;
    }
    auto* qualType = addEntity<QualType>(const_cast<ObjectType*>(base), ref);
    _qualTypes.insert({ key, qualType });
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
    return qualify(type->base(), ref);
}

QualType const* SymbolTable::arrayView(ObjectType const* base, Reference ref) {
    return qualify(arrayType(base, ArrayType::DynamicCount), ref);
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

QualType const* SymbolTable::qVoid(Reference ref) {
    return qualify(_void, ref);
}

QualType const* SymbolTable::qByte(Reference ref) {
    return qualify(_bool, ref);
}

QualType const* SymbolTable::qBool(Reference ref) {
    return qualify(_bool, ref);
}

QualType const* SymbolTable::qS8(Reference ref) { return qualify(_s8, ref); }

QualType const* SymbolTable::qS16(Reference ref) { return qualify(_s16, ref); }

QualType const* SymbolTable::qS32(Reference ref) { return qualify(_s32, ref); }

QualType const* SymbolTable::qS64(Reference ref) { return qualify(_s64, ref); }

QualType const* SymbolTable::qU8(Reference ref) { return qualify(_u8, ref); }

QualType const* SymbolTable::qU16(Reference ref) { return qualify(_u16, ref); }

QualType const* SymbolTable::qU32(Reference ref) { return qualify(_u32, ref); }

QualType const* SymbolTable::qU64(Reference ref) { return qualify(_u64, ref); }

QualType const* SymbolTable::qFloat(Reference ref) {
    return qualify(_float, ref);
}

template <typename E, typename... Args>
E* SymbolTable::addEntity(Args&&... args) {
    auto owner   = allocate<E>(std::forward<Args>(args)...);
    auto* result = owner.get();
    _entities.push_back(std::move(owner));
    return result;
}
