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
#include <range/v3/numeric.hpp>
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

    void analyzeImpl(ArrayType& type);

    LifetimeOperation resolveArrayOp(SMFKind kind, ArrayType& type);

    void analyzeImpl(UniquePtrType& type);

    void analyzeImpl(StructType& type);

    Function* findSMF(SMFKind kind,
                      StructType& type,
                      std::span<Function* const> functions);

    LifetimeOperation resolveStructOp(SMFKind kind,
                                      Function* userDefined,
                                      StructType& type,
                                      bool generateCondition);

    ///
    Function* generateSMF(SMFKind kind, StructType& type);

    /// \Returns the function type for the lifetime operation \p kind on type \p
    /// type
    FunctionType const* makeSMFType(SMFKind kind, StructType& type);
};

} // namespace

namespace {

enum class SMFAvail { NotAnalyzable, Deleted, Available, Trivial };

std::strong_ordering operator<=>(SMFAvail a, SMFAvail b) {
    return (int)a <=> (int)b;
}

} // namespace

///
static SMFAvail SMFOperationAvail(SMFKind kind, Type const* type) {
    if (!type) {
        return SMFAvail::NotAnalyzable;
    }
    // clang-format off
    return SC_MATCH (*type) {
        [&](ReferenceType const&) {
            return SMFAvail::Trivial;
        },
        [&](FunctionType const&) -> SMFAvail {
            SC_UNREACHABLE();
        },
        [&](ObjectType const& type) {
            auto* md = type.lifetimeMetadata();
            if (!md) {
                return SMFAvail::Trivial;
            }
            auto op = md->operation(kind);
            if (op.isTrivial()) {
                return SMFAvail::Trivial;
            }
            if (!op.isDeleted()) {
                return SMFAvail::Available;
            }
            return SMFAvail::Deleted;
        }
    }; // clang-format on
}

void LifetimeAnalyzer::analyzeImpl(ArrayType& type) {
    auto* elemType = type.elementType();
    SC_ASSERT(elemType, "Should not be instantiated with null");
    using enum SMFKind;
    type.setLifetimeMetadata({ resolveArrayOp(DefaultConstructor, type),
                               resolveArrayOp(CopyConstructor, type),
                               resolveArrayOp(MoveConstructor, type),
                               resolveArrayOp(Destructor, type) });
}

LifetimeOperation LifetimeAnalyzer::resolveArrayOp(SMFKind kind,
                                                   ArrayType& type) {
    auto elemAvail = SMFOperationAvail(kind, type.elementType());
    using enum SMFAvail;
    switch (elemAvail) {
    case NotAnalyzable:
        // TODO: Perhaps return a poison function here?
        return LifetimeOperation::Trivial;
    case Deleted:
        return LifetimeOperation::Deleted;
    case Available:
        return LifetimeOperation::NontrivialInline;
    case Trivial:
        return LifetimeOperation::Trivial;
    }
}

void LifetimeAnalyzer::analyzeImpl(UniquePtrType& type) { SC_UNIMPLEMENTED(); }

/// This also exists in ExpressionAnalysis. We might want to make this a member
/// of `Scope`
static utl::small_vector<Function*> findFunctions(Scope& scope,
                                                  std::string_view name) {
    auto entities = scope.findEntities(name);
    return entities | transform(cast<Function*>) | ToSmallVector<>;
}

void LifetimeAnalyzer::analyzeImpl(StructType& type) {
    auto newFns = findFunctions(type, "new");
    auto moveFns = findFunctions(type, "move");
    auto deleteFns = findFunctions(type, "delete");
    using enum SMFKind;

    Function* userDefCtor = findSMF(DefaultConstructor, type, newFns);
    Function* userCopyCtor = findSMF(CopyConstructor, type, newFns);
    Function* userMoveCtor = findSMF(MoveConstructor, type, moveFns);
    Function* userDtor = findSMF(Destructor, type, deleteFns);

    bool genLifetime = !userCopyCtor && !userMoveCtor && !userDtor;
    bool genDefCtor = genLifetime && newFns.empty();
    type.setLifetimeMetadata(
        { resolveStructOp(DefaultConstructor, userDefCtor, type, genDefCtor),
          resolveStructOp(CopyConstructor, userCopyCtor, type, genLifetime),
          resolveStructOp(MoveConstructor, userMoveCtor, type, genLifetime),
          resolveStructOp(Destructor, userDtor, type, genLifetime) });
}

void sema::analyzeLifetime(ObjectType& type, SymbolTable& sym) {
    LifetimeAnalyzer(sym).analyze(type);
}

Function* LifetimeAnalyzer::findSMF(SMFKind kind,
                                    StructType& type,
                                    std::span<Function* const> functions) {
    auto* funcType = makeSMFType(kind, type);
    return findBySignature(functions, funcType->argumentTypes());
}

LifetimeOperation LifetimeAnalyzer::resolveStructOp(SMFKind kind,
                                                    Function* userDef,
                                                    StructType& type,
                                                    bool generateCond) {
    if (userDef) {
        return userDef;
    }
    if (!generateCond) {
        return LifetimeOperation::Deleted;
    }
    using enum SMFAvail;
    auto memberOpAvail = type.members() |
                         transform(std::bind_front(SMFOperationAvail, kind));
    auto maxAvail = ranges::accumulate(memberOpAvail, Trivial, ranges::min);
    switch (maxAvail) {
    case NotAnalyzable:
        // TODO: Perhaps return a poison function here?
        return LifetimeOperation::Trivial;
    case Deleted:
        return LifetimeOperation::Deleted;
    case Available:
        return generateSMF(kind, type);
    case Trivial:
        return LifetimeOperation::Trivial;
    }
}

Function* LifetimeAnalyzer::generateSMF(SMFKind kind, StructType& type) {
    Function* function = sym.withScopeCurrent(&type, [&] {
        return sym.declareFunction(toSpelling(kind),
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
