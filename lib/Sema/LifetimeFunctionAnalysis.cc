/// ## Terminology
/// - **Lifetime function** The copy constructor, the move constructor, the
///   (synthesized) copy and move assignment operators and the destructor are
///   called _lifetime functions_
///
/// - **Special member function** The default constructor and the lifetime
///   functions are called _special member functions_
///
/// - **Trivial lifetime** A type is said to have _trivial lifetime_ if no
///   lifetime function is user defined and all non-static data members have
///   trivial lifetime
///
///   - All builtin types except for unique pointers have trivial lifetime
///
///   - Array types have trivial lifetime iff the element type has trivial
///     lifetime
///
/// If a type has non-trivial lifetime it requires a implicitly or explicitly
/// defined special member function for every lifetime operation and for default
/// construction
///
/// Special member functions can be implicitly defined if certain conditions
/// hold true:
///
/// ## Implicitly defined special member functions
///
/// - **Default constructor** `new(&mut this)`
///   Implicitly defined if no contructor or destructor is user defined and if
///   all data members have a default constructor
///
/// - **Copy constructor** `new(&mut this, other: &X)`
///   Implicitly defined if neither destructor or move constructor is user
///   defined and if all data members have a copy constructor
///
/// - **Copy assignment operator** (no syntax, only ever synthesized)
///   Implicitly derived from copy constructor if that is defined
///
/// - **Move constructor** `move(&mut this, other: &mut X)`
///   Implicitly defined if neither destructor or copy constructor is user
///   defined and if all data members have a move constructor
///
/// - **Move assignment operator** (no syntax, only ever synthesized)
///   Implicitly derived from move constructor if that is defined
///
/// - **Destructor** `delete(&mut this)`
///   Always implicitly defined if not user defined
///
/// - **Aggregate 'constructor'** (used in expression like `X(1, 2.0, true)`
///   Implicitly defined if no contructor or destructor is user defined
///

#include "Sema/LifetimeFunctionAnalysis.h"

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>

#include "Common/Ranges.h"
#include "Sema/Analysis/Utility.h"
#include "Sema/Entity.h"
#include "Sema/LifetimeMetadata.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using namespace ranges::views;

namespace {

struct LifetimeAnalyzer {
    SymbolTable& sym;

    LifetimeAnalyzer(SymbolTable& sym): sym(sym) {}

    void analyze(ObjectType& type) {
        visit(type, [this](auto& type) { analyzeImpl(type); });
    }

    void analyzeImpl(ObjectType&) { SC_UNREACHABLE(); }

    void analyzeImpl(StructType& type);

    Function* findOrGenerate(SMFKind kind,
                             StructType& type,
                             std::span<Function* const> ctors,
                             bool anyUserDefined);
    Function* generateSMF(SMFKind kind, StructType& type);
    FunctionType const* makeSMFType(SMFKind kind, StructType& type);
};

} // namespace

static bool hasSMFOperation(SMFKind kind, Type const* type) {
    SC_UNIMPLEMENTED();
}

static utl::small_vector<Function*> findFunctions(Scope& scope,
                                                  std::string_view name) {
    auto entities = scope.findEntities(name);
    return entities | transform(cast<Function*>) | ToSmallVector<>;
}

static FunctionType const* makeLifetimeSignature(
    SpecialLifetimeFunctionDepr key, ObjectType& type, SymbolTable& sym) {
    auto* self = sym.reference(QualType::Mut(&type));
    auto* rhs = sym.reference(QualType::Const(&type));
    auto* ret = sym.Void();
    using enum SpecialLifetimeFunctionDepr;
    switch (key) {
    case DefaultConstructor:
        return sym.functionType({ self }, ret);
    case CopyConstructor:
        return sym.functionType({ self, rhs }, ret);
    case MoveConstructor:
        return sym.functionType({ self, rhs }, ret);
    case Destructor:
        return sym.functionType({ self }, ret);
    }
    SC_UNREACHABLE();
}

void LifetimeAnalyzer::analyzeImpl(StructType& type) {
    auto newFns = findFunctions(type, "new");
    auto moveFns = findFunctions(type, "move");
    auto deleteFns = findFunctions(type, "delete");
    bool anyUserDefined = !newFns.empty() || !moveFns.empty() ||
                          !deleteFns.empty();
    using enum SMFKind;
    Function* defCtor =
        findOrGenerate(DefaultConstructor, type, newFns, anyUserDefined);
    Function* copyCtor =
        findOrGenerate(CopyConstructor, type, newFns, anyUserDefined);
    Function* moveCtor =
        findOrGenerate(MoveConstructor, type, moveFns, anyUserDefined);
    Function* dtorCtor =
        findOrGenerate(Destructor, type, deleteFns, anyUserDefined);
    type.setLifetimeMetadata({ defCtor, copyCtor, moveCtor, dtorCtor });
    if (newFns.empty()) {
        newFns = { defCtor, copyCtor };
    }
    type.setLifetimeFunctions(newFns, moveCtor, dtorCtor);
}

void sema::analyzeLifetime(ObjectType& type, SymbolTable& sym) {
    LifetimeAnalyzer(sym).analyze(type);
}

Function* LifetimeAnalyzer::findOrGenerate(SMFKind kind,
                                           StructType& type,
                                           std::span<Function* const> functions,
                                           bool anyUserDefined) {
    auto* funcType = makeSMFType(kind, type);
    Function* fn = findBySignature(functions, funcType->argumentTypes());
    if (fn) {
        return fn;
    }
    if (!anyUserDefined &&
        ranges::all_of(type.members(), std::bind_front(hasSMFOperation, kind)))
    {
        return generateSMF(kind, type);
    }
    return nullptr;
}

Function* LifetimeAnalyzer::generateSMF(SMFKind kind, StructType& type) {
    Function* function = sym.withScopeCurrent(&type, [&] {
        return sym.declareFunction(toName(kind),
                                   makeSMFType(kind, type),
                                   type.accessControl());
    });
    SC_ASSERT(function, "Name can't be used by other symbol");
    function->setKind(FunctionKind::Generated);
    function->setSMFKind(kind);
    return function;
}

FunctionType const* LifetimeAnalyzer::makeSMFType(SMFKind kind,
                                                  StructType& type) {
    auto* self = sym.reference(QualType::Mut(&type));
    auto* rhs = sym.reference(QualType::Const(&type));
    auto* ret = sym.Void();
    using enum SMFKind;
    switch (kind) {
    case DefaultConstructor:
        return sym.functionType({ self }, ret);
    case CopyConstructor:
        return sym.functionType({ self, rhs }, ret);
    case MoveConstructor:
        return sym.functionType({ self, rhs }, ret);
    case Destructor:
        return sym.functionType({ self }, ret);
    }
    SC_UNREACHABLE();
}
