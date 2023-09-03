#include "Sema/Analysis/Instantiation.h"

#include <array>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Common/Base.h"
#include "Common/Ranges.h"
#include "Sema/Analysis/ExpressionAnalysis.h"
#include "Sema/Analysis/StructDependencyGraph.h"
#include "Sema/Entity.h"
#include "Sema/QualType.h"
#include "Sema/SemanticIssue.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct Context {
    std::vector<StructureType const*> instantiateTypes(
        StructDependencyGraph& typeDependencies);

    void instantiateFunctions(std::span<ast::FunctionDefinition*> functions);

    void instantiateStructureType(SDGNode&);

    void instantiateVariable(SDGNode&);

    void instantiateFunction(ast::FunctionDefinition&);

    FunctionSignature analyzeSignature(ast::FunctionDefinition&) const;

    QualType analyzeParameter(ast::ParameterDeclaration&, size_t index) const;

    void generateSpecialLifetimeFunctions(StructureType& type);

    QualType analyzeTypeExpression(ast::Expression&) const;

    Function* generateSpecialLifetimeFunction(SpecialLifetimeFunction key,
                                              StructureType& type) const;

    FunctionSignature makeLifetimeSignature(
        StructureType& type, SpecialLifetimeFunction function) const;

    SymbolTable& sym;
    IssueHandler& iss;
};

} // namespace

std::vector<StructureType const*> sema::instantiateEntities(
    SymbolTable& sym,
    IssueHandler& iss,
    StructDependencyGraph& typeDependencies,
    std::span<ast::FunctionDefinition*> functions) {
    Context ctx{ .sym = sym, .iss = iss };
    auto structs = ctx.instantiateTypes(typeDependencies);
    for (auto* def: functions) {
        ctx.instantiateFunction(*def);
    }
    /// `structs` is topologically sorted, so each invocation of
    /// `generateSpecialLifetimeFunctions()` can assume that the types of all
    /// data members already have been analyzed for lifetime triviality
    for (auto* type: structs) {
        ctx.generateSpecialLifetimeFunctions(const_cast<StructureType&>(*type));
    }
    return structs;
}

static bool isUserDefined(QualType type) {
    // clang-format off
    return SC_MATCH(*type) {
        [](StructureType const&) {
            return true;
        },
        [](ArrayType const& arrayType) {
            return isUserDefined(arrayType.elementType());
        },
        [](ObjectType const&) {
            return false;
        }
    }; // clang-format on
}

std::vector<StructureType const*> Context::instantiateTypes(
    StructDependencyGraph& dependencyGraph) {
    /// After gather name phase we have the names of all types in the symbol
    /// table and we gather the dependencies of variable declarations in
    /// structs. This must be done before sorting the dependency graph.
    auto dataMembers = dependencyGraph | ranges::views::filter([](auto& node) {
                           return isa<Variable>(node.entity);
                       });
    for (auto& node: dataMembers) {
        auto& var = cast<ast::VariableDeclaration&>(*node.astNode);
        QualType type = analyzeTypeExpression(*var.typeExpr());
        if (type && isUserDefined(type)) {
            node.dependencies.push_back(
                utl::narrow_cast<u16>(dependencyGraph.index(type.get())));
        }
    }
    /// Check for cycles
    auto indices = ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
                   ranges::to<utl::small_vector<u16>>;
    auto const cycle =
        utl::find_cycle(indices.begin(),
                        indices.end(),
                        [&](size_t index) -> auto const& {
                            return dependencyGraph[index].dependencies;
                        });
    if (!cycle.empty()) {
        using Node = StrongReferenceCycle::Node;
        auto nodes = cycle | ranges::views::transform([&](size_t index) {
                         auto const& node = dependencyGraph[index];
                         return Node{ node.astNode, node.entity };
                     }) |
                     ranges::to<utl::vector>;
        iss.push<StrongReferenceCycle>(std::move(nodes));
        return {};
    }
    /// Sort dependencies...
    auto dependencyTraversalOrder =
        ranges::views::iota(size_t{ 0 }, dependencyGraph.size()) |
        ranges::to<utl::small_vector<u16>>;
    utl::topsort(dependencyTraversalOrder.begin(),
                 dependencyTraversalOrder.end(),
                 [&](size_t index) -> auto const& {
                     return dependencyGraph[index].dependencies;
                 });
    std::vector<StructureType const*> sortedStructTypes;
    /// Instantiate all types and member variables.
    for (size_t const index: dependencyTraversalOrder) {
        auto& node = dependencyGraph[index];
        // clang-format off
        visit(*node.entity, utl::overload{
            [&](Variable const&) { instantiateVariable(node); },
            [&](StructureType const& type) {
                instantiateStructureType(node);
                sortedStructTypes.push_back(&type);
            },
            [&](Entity const&) { SC_UNREACHABLE(); }
        }); // clang-format on
    }
    return sortedStructTypes;
}

void Context::instantiateFunctions(
    std::span<ast::FunctionDefinition*> functions) {}

static std::optional<SpecialMemberFunction> toSMF(
    ast::FunctionDefinition const& funcDef) {
    for (uint8_t i = 0; i < EnumSize<SpecialMemberFunction>; ++i) {
        SpecialMemberFunction SMF{ i };
        if (toString(SMF) == funcDef.name()) {
            return SMF;
        }
    }
    return std::nullopt;
}

void Context::instantiateStructureType(SDGNode& node) {
    ast::StructDefinition& structDef =
        cast<ast::StructDefinition&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope([&] { sym.makeScopeCurrent(nullptr); });
    size_t objectSize = 0;
    size_t objectAlign = 0;
    auto& structType = cast<StructureType&>(*structDef.entity());
    /// Here we collect all the variables and special member function of the
    /// struct
    for (auto [varIndex, varDecl]: structDef.body()->statements() |
                                       Filter<ast::VariableDeclaration> |
                                       ranges::views::enumerate)
    {
        auto& var = *varDecl->variable();
        structType.addMemberVariable(&var);
        if (!varDecl->type()) {
            return;
        }
        QualType varType = varDecl->type();
        objectAlign = std::max(objectAlign, varType->align());
        SC_ASSERT(varType->size() % varType->align() == 0,
                  "size must be a multiple of align");
        objectSize = utl::round_up_pow_two(objectSize, varType->align());
        size_t const currentOffset = objectSize;
        varDecl->setOffset(currentOffset);
        varDecl->setIndex(varIndex);
        var.setOffset(currentOffset);
        var.setIndex(varIndex);
        objectSize += varType->size();
    }
    structType.setSize(objectSize);
    structType.setAlign(objectAlign);
}

void Context::instantiateVariable(SDGNode& node) {
    ast::VariableDeclaration& varDecl =
        cast<ast::VariableDeclaration&>(*node.astNode);
    sym.makeScopeCurrent(node.entity->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    QualType type = analyzeTypeExpression(*varDecl.typeExpr());
    varDecl.decorate(node.entity, type);
    /// Here we set the TypeID of the variable in the symbol table.
    varDecl.variable()->setType(type);
}

static bool isExplicitRefTo(QualType argType, QualType referredType) {
    return isExplRef(argType) && stripReference(argType) == referredType;
}

void Context::instantiateFunction(ast::FunctionDefinition& def) {
    auto* function = def.function();
    sym.makeScopeCurrent(function->parent());
    utl::armed_scope_guard popScope = [&] { sym.makeScopeCurrent(nullptr); };
    auto result = sym.setSignature(function, analyzeSignature(def));
    if (!result) {
        if (auto* invStatement =
                dynamic_cast<InvalidStatement*>(result.error()))
        {
            invStatement->setStatement(def);
        }
        iss.push(result.error());
        return;
    }
    auto SMF = toSMF(def);
    if (!SMF) {
        return;
    }
    auto pushError = [&] {
        using enum InvalidDeclaration::Reason;
        iss.push(new InvalidDeclaration(&def,
                                        InvalidSpecialMemberFunction,
                                        *function->parent()));
    };
    auto* structType = dyncast<StructureType*>(function->parent());
    if (!structType) {
        pushError();
        return;
    }
    function->setSMFKind(*SMF);
    structType->addSpecialMemberFunction(*SMF, function->overloadSet());
    auto const& sig = function->signature();
    if (sig.argumentCount() == 0 ||
        !isExplicitRefTo(sig.argumentType(0), structType))
    {
        pushError();
        return;
    }
    using enum SpecialMemberFunction;
    switch (function->SMFKind()) {
    case New:
        break;
    case Move:
        if (sig.argumentCount() != 2 ||
            !isExplicitRefTo(sig.argumentType(1), structType))
        {
            pushError();
            return;
        }
        break;
    case Delete:
        if (sig.argumentCount() != 1) {
            pushError();
            return;
        }
        break;
    }
}

FunctionSignature Context::analyzeSignature(
    ast::FunctionDefinition& decl) const {
    utl::small_vector<QualType> argumentTypes;
    for (auto [index, param]: decl.parameters() | ranges::views::enumerate) {
        argumentTypes.push_back(analyzeParameter(*param, index));
    }
    /// For functions with unspecified return type we assume void until we
    /// implement return type deduction.
    QualType returnType = decl.returnTypeExpr() ?
                              analyzeTypeExpression(*decl.returnTypeExpr()) :
                              sym.Void();
    return FunctionSignature(std::move(argumentTypes), returnType);
}

QualType Context::analyzeParameter(ast::ParameterDeclaration& param,
                                   size_t index) const {
    auto* thisParam = dyncast<ast::ThisParameter const*>(&param);
    if (!thisParam) {
        return analyzeTypeExpression(*param.typeExpr());
    }
    if (index != 0) {
        iss.push<InvalidDeclaration>(&param,
                                     InvalidDeclaration::Reason::ThisParameter,
                                     sym.currentScope());
        return nullptr;
    }
    auto* structure = dyncast<ast::StructDefinition const*>(
        param.parent()->parent()->parent());
    if (!structure) {
        iss.push<InvalidDeclaration>(&param,
                                     InvalidDeclaration::Reason::ThisParameter,
                                     sym.currentScope());
        return nullptr;
    }
    auto* structType = cast<StructureType const*>(structure->entity());
    if (auto ref = thisParam->reference()) {
        return sym.explRef(QualType(structType, thisParam->mutability()));
    }
    return structType;
}

static bool isRefTo(QualType type, QualType referred) {
    if (auto* refType = dyncast<ReferenceType const*>(type.get())) {
        return refType->base().get() == referred;
    }
    return false;
}

namespace {

/// Wrapper around `std::array` that can be index by `SpecialLifetimeFunction`
/// enum values
struct SLFArray: std::array<Function*, EnumSize<SpecialLifetimeFunction>> {
    using Base = std::array<Function*, EnumSize<SpecialLifetimeFunction>>;
    auto& operator[](SpecialLifetimeFunction index) {
        return Base::operator[](static_cast<size_t>(index));
    }
    auto const& operator[](SpecialLifetimeFunction index) const {
        return Base::operator[](static_cast<size_t>(index));
    }
};

} // namespace

static SLFArray getDefinedSLFs(StructureType& type) {
    using enum SpecialMemberFunction;
    using enum SpecialLifetimeFunction;
    SLFArray result{};
    if (auto* constructor = type.specialMemberFunction(New)) {
        for (auto* function: *constructor) {
            auto const& sig = function->signature();
            switch (sig.argumentCount()) {
            case 1: {
                if (isRefTo(sig.argumentType(0), QualType::Mut(&type))) {
                    result[DefaultConstructor] = function;
                }
                break;
            }
            case 2: {
                if (isRefTo(sig.argumentType(0), QualType::Mut(&type)) &&
                    isRefTo(sig.argumentType(1), QualType::Const(&type)))
                {
                    result[CopyConstructor] = function;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    if (auto* move = type.specialMemberFunction(Move)) {
        SC_ASSERT(move->size() == 1, "Can only have one move constructor");
        result[MoveConstructor] = move->front();
    }
    if (auto* del = type.specialMemberFunction(Delete)) {
        SC_ASSERT(del->size() == 1, "Can only have one destructor");
        result[Destructor] = del->front();
    }
    return result;
}

static bool computeDefaultConstructible(StructureType& type,
                                        SLFArray const& SLF) {
    using enum SpecialLifetimeFunction;
    using enum SpecialMemberFunction;
    /// If we have a default constructor we are clearly default constructible
    if (SLF[DefaultConstructor]) {
        return true;
    }
    auto* overloadSet = type.specialMemberFunction(New);
    /// If no constructors are defined or if the only defined constructor is the
    /// copy constructor, then the type is default constructible iff. all member
    /// variables are default constructible
    if (!overloadSet || (overloadSet->size() == 1 && SLF[CopyConstructor])) {
        return ranges::all_of(type.memberVariables(), [](auto* var) {
            return var->type()->isDefaultConstructible();
        });
    }
    /// Otherwise we are not default constructible
    return false;
}

static bool computeTrivialLifetime(StructureType& type, SLFArray const& SLF) {
    using enum SpecialLifetimeFunction;
    return !SLF[CopyConstructor] && !SLF[MoveConstructor] && !SLF[Destructor] &&
           ranges::all_of(type.memberVariables(), [](auto* var) {
               return var->type()->hasTrivialLifetime();
           });
}

void Context::generateSpecialLifetimeFunctions(StructureType& type) {
    using enum SpecialLifetimeFunction;
    auto SLF = getDefinedSLFs(type);
    bool const isDefaultConstructible = computeDefaultConstructible(type, SLF);
    bool const hasTrivialLifetime = computeTrivialLifetime(type, SLF);
    type.setIsDefaultConstructible(isDefaultConstructible);
    type.setHasTrivialLifetime(hasTrivialLifetime);
    if (isDefaultConstructible && !SLF[DefaultConstructor]) {
        bool anyMemberHasDefCtor = ranges::any_of(type.memberVariables(),
                                                  [](auto* var) {
            auto* type = dyncast<StructureType const*>(var->type().get());
            return type && type->specialLifetimeFunction(DefaultConstructor);
        });
        if (anyMemberHasDefCtor) {
            SLF[DefaultConstructor] =
                generateSpecialLifetimeFunction(DefaultConstructor, type);
        }
    }
    if (!hasTrivialLifetime) {
        if (!SLF[CopyConstructor]) {
            SLF[CopyConstructor] =
                generateSpecialLifetimeFunction(CopyConstructor, type);
        }
        if (!SLF[MoveConstructor]) {
            SLF[MoveConstructor] =
                generateSpecialLifetimeFunction(MoveConstructor, type);
        }
        if (!SLF[Destructor]) {
            SLF[Destructor] = generateSpecialLifetimeFunction(Destructor, type);
        }
    }
    type.setSpecialLifetimeFunctions(SLF);
}

Function* Context::generateSpecialLifetimeFunction(SpecialLifetimeFunction key,
                                                   StructureType& type) const {
    auto SMFKind = toSMF(key);
    Function* function = sym.withScopeCurrent(&type, [&] {
        return &sym.declareFunction(std::string(toString(SMFKind))).value();
    });
    sym.setSignature(function, makeLifetimeSignature(type, key));
    function->setKind(FunctionKind::Generated);
    function->setIsMember();
    function->setSMFKind(SMFKind);
    type.addSpecialMemberFunction(SMFKind, function->overloadSet());
    return function;
}

FunctionSignature Context::makeLifetimeSignature(
    StructureType& type, SpecialLifetimeFunction function) const {
    QualType self = sym.explRef(QualType::Mut(&type));
    QualType rhs = sym.explRef(QualType::Const(&type));
    QualType ret = sym.Void();
    using enum SpecialLifetimeFunction;
    switch (function) {
    case DefaultConstructor:
        return FunctionSignature({ self }, ret);
    case CopyConstructor:
        return FunctionSignature({ self, rhs }, ret);
    case MoveConstructor:
        return FunctionSignature({ self, rhs }, ret);
    case Destructor:
        return FunctionSignature({ self }, ret);
    }
}

QualType Context::analyzeTypeExpression(ast::Expression& expr) const {
    DTorStack dtorStack;
    if (!sema::analyzeExpression(expr, dtorStack, sym, iss)) {
        return nullptr;
    }
    SC_ASSERT(dtorStack.empty(), "");
    if (!expr.isType()) {
        iss.push<BadSymbolReference>(expr, EntityCategory::Type);
        return nullptr;
    }
    return cast<ObjectType*>(expr.entity());
}
