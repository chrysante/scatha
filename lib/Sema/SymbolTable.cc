#include "Sema/SymbolTable.h"

#include <string>

#include "Sema/SemanticIssue.h"

using namespace scatha;
using namespace sema;

SymbolTable::SymbolTable():
    _globalScope(std::make_unique<GlobalScope>()),
    _currentScope(_globalScope.get())
{
    /// Declare \p void with \p invalidSize  to make it an incomplete type.
    _void   = declareBuiltinType("void", invalidSize, invalidSize);
    _bool   = declareBuiltinType("bool", 1, 1);
    _int    = declareBuiltinType("int", 8, 8);
    _float  = declareBuiltinType("float", 8, 8);
    _string = declareBuiltinType("string", sizeof(std::string), alignof(std::string));
}

Expected<ObjectType&, SemanticIssue> SymbolTable::declareObjectType(ast::StructDefinition const& structDef) {
    auto result = declareObjectType(structDef.token());
    if (!result) {
        result.error().setStatement(structDef);
    }
    return result;
}

Expected<ObjectType&, SemanticIssue> SymbolTable::declareObjectType(Token name) {
    using enum InvalidDeclaration::Reason;
    if (name.isKeyword) {
        return SemanticIssue{
            InvalidDeclaration(nullptr, ReservedIdentifier, currentScope(), SymbolCategory::ObjectType)
        };
    }
    const SymbolID symbolID = currentScope().findID(name.id);
    if (symbolID != SymbolID::Invalid) {
        return SemanticIssue{
            InvalidDeclaration(nullptr, Redefinition, currentScope(), SymbolCategory::ObjectType, categorize(symbolID))
        };
    }
    auto [itr, success] = _objectTypes.insert(ObjectType(name.id,
                                                         generateID(),
                                                         &currentScope()));
    SC_ASSERT(success, "");
    currentScope().add(*itr);
    return *itr;
}

TypeID SymbolTable::declareBuiltinType(std::string name, size_t size, size_t align) {
    auto result = declareObjectType(Token(name));
    SC_ASSERT(result, "How could this fail?");
    result->setSize(size);
    result->setAlign(align);
    result->setIsBuiltin(true);
    return result->symbolID();
}

Expected<Function const&, SemanticIssue> SymbolTable::declareFunction(ast::FunctionDefinition const& functionDef) {
    auto result = declareFunction(functionDef.token());
    if (!result) {
        result.error().setStatement(functionDef);
    }
    return result;
}

Expected<Function const&, SemanticIssue> SymbolTable::declareFunction(Token name) {
    using enum InvalidDeclaration::Reason;
    if (name.isKeyword) {
        return SemanticIssue{ InvalidDeclaration(nullptr,
                                                 ReservedIdentifier,
                                                 currentScope(),
                                                 SymbolCategory::Function) };
    }
    SymbolID const overloadSetID = [&] {
        auto const id = currentScope().findID(name.id);
        if (id != SymbolID::Invalid) {
            return id;
        }
        /// Create a new overload set
        auto [itr, success] = _overloadSets.insert(OverloadSet(name.id, generateID(), &currentScope()));
        SC_ASSERT(success, "");
        currentScope().add(*itr);
        return itr->symbolID();
    }();
    /// Even if we get a valid ID from the lambda, we don't know yet if it's really an overload set.
    auto* const overloadSetPtr = tryGetOverloadSet(overloadSetID);
    if (!overloadSetPtr) {
        return SemanticIssue{ InvalidDeclaration(nullptr,
                                                 InvalidDeclaration::Reason::Redefinition,
                                                 currentScope(),
                                                 SymbolCategory::Function,
                                                 categorize(overloadSetID)) };
    }
    auto const [itr, success] = _functions.insert(Function(name.id,
                                                           /* functionID = */ generateID(),
                                                           /* overloadSetID = */ overloadSetID,
                                                           &currentScope()));
    SC_ASSERT(success, "?");
    Function& function = *itr;
    currentScope().add(function);
    return *itr;
}

Expected<void, SemanticIssue> SymbolTable::setSignature(SymbolID functionID, FunctionSignature sig) {
    auto& function = getFunction(functionID);
    auto const overloadSetID = function.overloadSetID();
    auto osItr = _overloadSets.find(overloadSetID);
    SC_EXPECT(osItr != _overloadSets.end(), "");
    auto& overloadSet = *osItr;
    function._sig = std::move(sig);
    auto const [otherFunction, success] = overloadSet.add(&function);
    if (!success) {
        using enum InvalidDeclaration::Reason;
        InvalidDeclaration::Reason const reason =
            otherFunction->signature().returnTypeID() == function.signature().returnTypeID() ? Redefinition : CantOverloadOnReturnType;
        return SemanticIssue{ InvalidDeclaration(nullptr, reason, currentScope(), SymbolCategory::Function) };
    }
    return {};
}

Expected<Variable&, SemanticIssue> SymbolTable::declareVariable(ast::VariableDeclaration const& varDecl) {
    auto result = declareVariable(varDecl.token());
    if (!result) {
        result.error().setStatement(varDecl);
    }
    return result;
}

Expected<Variable&, SemanticIssue> SymbolTable::declareVariable(Token name) {
    using enum InvalidDeclaration::Reason;
    if (name.isKeyword) {
        return SemanticIssue{
            InvalidDeclaration(nullptr, ReservedIdentifier, currentScope(), SymbolCategory::Variable)
        };
    }
    const SymbolID symbolID = currentScope().findID(name.id);
    if (symbolID != SymbolID::Invalid) {
        return SemanticIssue{
            InvalidDeclaration(nullptr, Redefinition, currentScope(), SymbolCategory::Variable, categorize(symbolID))
        };
    }
    auto [itr, success] = _variables.insert(Variable(name.id, generateID(), &currentScope()));
    SC_ASSERT(success, "");
    currentScope().add(*itr);
    return *itr;
}

Expected<Variable&, SemanticIssue> SymbolTable::addVariable(ast::VariableDeclaration const& varDecl,
                                                            TypeID typeID,
                                                            size_t offset)
{
    auto result = addVariable(varDecl.token(), typeID, offset);
    if (!result) {
        result.error().setStatement(varDecl);
    }
    return result;
}

Expected<Variable&, SemanticIssue> SymbolTable::addVariable(Token name,
                                                            TypeID typeID,
                                                            size_t offset)
{
    auto declResult = declareVariable(std::move(name));
    if (!declResult) {
        return declResult.error();
    }
    auto& var = *declResult;
    var.setTypeID(typeID);
    SC_ASSERT(offset == 0, "Temporary measure. Should remove parameter offset");
    return var;
}

Scope const& SymbolTable::addAnonymousScope() {
    auto [itr, success] = _anonymousScopes.insert(Scope(ScopeKind::Function, generateID(), &currentScope()));
    SC_ASSERT(success, "");
    currentScope().add(*itr);
    return *itr;
}

void SymbolTable::pushScope(SymbolID id) {
    auto const itr = currentScope()._children.find(id);
    SC_ASSERT(itr != currentScope()._children.end(), "not found");
    _currentScope = itr->second;
}

void SymbolTable::popScope() {
    _currentScope = currentScope().parent();
}

void SymbolTable::makeScopeCurrent(Scope* scope) {
    _currentScope = scope ? scope : &globalScope();
}

OverloadSet const& SymbolTable::getOverloadSet(SymbolID id) const {
    auto const* result = tryGetOverloadSet(id);
    SC_ASSERT(result, "ID must be valid and reference an overload set");
    return *result;
}

OverloadSet const* SymbolTable::tryGetOverloadSet(SymbolID id) const {
    auto const itr = _overloadSets.find(id);
    if (itr == _overloadSets.end()) {
        return nullptr;
    }
    return &*itr;
}

Function const& SymbolTable::getFunction(SymbolID id) const {
    auto const* result = tryGetFunction(id);
    SC_ASSERT(result, "ID must be valid and reference a function");
    return *result;
}

Function const* SymbolTable::tryGetFunction(SymbolID id) const {
    auto const itr = _functions.find(id);
    if (itr == _functions.end()) {
        return nullptr;
    }
    return &*itr;
}

Variable const& SymbolTable::getVariable(SymbolID id) const {
    auto const* result = tryGetVariable(id);
    SC_ASSERT(result, "ID must be valid and reference a variable");
    return *result;
}

Variable const* SymbolTable::tryGetVariable(SymbolID id) const {
    auto const itr = _variables.find(id);
    if (itr == _variables.end()) {
        return nullptr;
    }
    return &*itr;
}

ObjectType const& SymbolTable::getObjectType(SymbolID id) const {
    auto const* result = tryGetObjectType(id);
    SC_ASSERT(result, "ID must be valid and reference an object type");
    return *result;
}

ObjectType const* SymbolTable::tryGetObjectType(SymbolID id) const {
    auto const itr = _objectTypes.find(id);
    if (itr == _objectTypes.end()) {
        return nullptr;
    }
    return &*itr;
}

std::string SymbolTable::getName(SymbolID id) const {
    if (!id) {
        return "<invalid-id>";
    }
    auto const cat = categorize(id);
    switch (cat) {
    case SymbolCategory::Variable: {
        auto const* ptr = tryGetVariable(id);
        return ptr ? std::string(ptr->name()) : "<invalid-variable>";
    }
    case SymbolCategory::Namespace: {
        SC_DEBUGFAIL();
    }
    case SymbolCategory::OverloadSet: {
        auto const* ptr = tryGetOverloadSet(id);
        return ptr ? std::string(ptr->name()) : "<invalid-overloadset>";
    }
    case SymbolCategory::Function: {
        auto const* ptr = tryGetFunction(id);
        return ptr ? std::string(ptr->name()) : "<invalid-function>";
    }
    case SymbolCategory::ObjectType: {
        auto const* ptr = tryGetObjectType(id);
        return ptr ? std::string(ptr->name()) : "<invalid-type>";
    }
    default: SC_DEBUGFAIL();
    }
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

OverloadSet const* SymbolTable::lookupOverloadSet(std::string_view name) const {
    SymbolID const id = lookup(name);
    if (id == SymbolID::Invalid) {
        return nullptr;
    }
    return &getOverloadSet(id);
}

Variable const* SymbolTable::lookupVariable(std::string_view name) const {
    SymbolID const id = lookup(name);
    if (id == SymbolID::Invalid) {
        return nullptr;
    }
    return &getVariable(id);
}

ObjectType const* SymbolTable::lookupObjectType(std::string_view name) const {
    SymbolID const id = lookup(name);
    if (id == SymbolID::Invalid) {
        return nullptr;
    }
    return &getObjectType(id);
}

bool SymbolTable::is(SymbolID id, SymbolCategory cat) const {
    if (!std::has_single_bit(static_cast<unsigned>(cat))) {
        int const count = utl::log2(utl::to_underlying(SymbolCategory::_count) - 1);
        for (int i = 0; i < count; ++i) {
            if (test(SymbolCategory(1 << i) & cat) && is(id, SymbolCategory(1 << i))) {
                return true;
            }
        }
        return false;
    }

    using enum SymbolCategory;
    switch (cat) {
    case Variable: return _variables.contains(id);
    case Namespace: return false;
    case OverloadSet: return _overloadSets.contains(id);
    case Function: return _functions.contains(id);
    case ObjectType: return _objectTypes.contains(id);
    case _count: SC_DEBUGFAIL();
    }
}

SymbolCategory SymbolTable::categorize(SymbolID id) const {
    if (_variables.contains(id)) {
        return SymbolCategory::Variable;
    }
    if (_objectTypes.contains(id)) {
        return SymbolCategory::ObjectType;
    }
    if (_overloadSets.contains(id)) {
        return SymbolCategory::OverloadSet;
    }
    if (_functions.contains(id)) {
        return SymbolCategory::Function;
    }
    SC_DEBUGFAIL(); // What is this ID?
}

SymbolID SymbolTable::generateID() {
    return SymbolID(_idCounter++);
}
