#include "IRGen/IRGen.h"

#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/graph.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "Common/DebugInfo.h"
#include "Common/FileHandling.h"
#include "IR/CFG/Function.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/FunctionGeneration.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/Maps.h"
#include "IRGen/Metadata.h"
#include "Invocation/TargetNames.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;

/// Helper function to get a hashmap entry or abort
template <typename T>
static T* get(utl::hashmap<std::string, T*> const& map, std::string_view name) {
    auto itr = map.find(name);
    SC_RELASSERT(itr != map.end(),
                 utl::strcat("Failed to find symbol '", name, "' in library")
                     .c_str());
    return itr->second;
}

/// Performs a DFS over a library scope and adds entries for all structs and
/// functions to the type map and function map
static void mapLibSymbols(
    sema::Scope const& scope, TypeMap& typeMap, GlobalMap& globalMap,
    sema::NameMangler const& nameMangler,
    utl::hashmap<std::string, ir::StructType*> const& IRStructMap,
    utl::hashmap<std::string, ir::Global*> const& IRObjectMap) {
    auto entities = scope.entities() | ToSmallVector<>;
    for (auto* entity: entities | Filter<sema::StructType, sema::Function>) {
        // clang-format off
        SC_MATCH (*entity) {
            [&](sema::StructType const& semaType) {
                std::string name = nameMangler(*entity);
                typeMap.insert(&semaType,
                               get(IRStructMap, name),
                               makeStructMetadata(typeMap, &semaType));
            },
            [&](sema::Function const& semaFn) {
                std::string name = nameMangler(*entity);
                auto* irFn = cast<ir::Function*>(get(IRObjectMap, name));
                globalMap.insert(&semaFn,
                                 { irFn, computeCallingConvention(semaFn) });
            },
            [&]([[maybe_unused]] sema::Entity const& entity) {}
        }; // clang-format on
    }
    for (auto* child: scope.children()) {
        mapLibSymbols(*child, typeMap, globalMap, nameMangler, IRStructMap,
                      IRObjectMap);
    }
}

static void checkParserIssues(std::span<ir::ParseIssue const> issues,
                              std::string_view libName) {
    auto fatal = issues | filter([](ir::ParseIssue const& issue) {
        if (!std::holds_alternative<ir::SemanticIssue>(issue)) {
            return true;
        }
        return std::get<ir::SemanticIssue>(issue).reason() !=
               ir::SemanticIssue::Redeclaration;
    }) | ToSmallVector<>;
    if (fatal.empty()) {
        return;
    }
    std::cout << "Failed to parse library \"" << libName << "\":\n";
    for (auto& issue: fatal) {
        ir::print(issue);
    }
    SC_ABORT();
}

static void importLibrary(ir::Context& ctx, ir::Module& mod,
                          sema::NativeLibrary const& lib, TypeMap& typeMap,
                          GlobalMap& globalMap,
                          sema::NameMangler const& nameMangler) {
    auto archive = Archive::Open(lib.path());
    SC_RELASSERT(archive, "Failed to open library file");
    auto code = archive->openTextFile(TargetNames::ObjectCodeName);
    SC_RELASSERT(code, "Failed to open object code file");
    utl::hashmap<std::string, ir::StructType*> IRStructMap;
    utl::hashmap<std::string, ir::Global*> IRObjectMap;
    auto typeCallback = [&](ir::StructType& type) {
        IRStructMap.insert({ std::string(type.name()), &type });
    };
    auto objCallback = [&](ir::Global& object) {
        if (auto* irFn = dyncast<ir::Function*>(&object)) {
            irFn->setVisibility(ir::Visibility::Internal);
        }
        IRObjectMap.insert({ std::string(object.name()), &object });
    };
    auto parseIssues = ir::parseTo(*code, ctx, mod,
                                   { .typeParseCallback = typeCallback,
                                     .objectParseCallback = objCallback });
    checkParserIssues(parseIssues, lib.path().string());
    mapLibSymbols(lib, typeMap, globalMap, nameMangler, IRStructMap,
                  IRObjectMap);
}

/// \Returns `true` for all functions that are generated unconditionally, i.e.
/// even if they are not called by other functions
static bool initialDeclFilter(sema::Function const* function) {
    return mapVisibility(function) == ir::Visibility::External;
}

/// \Returns `true` if the entity \p entity is defined in an imported library
static bool isLibEntity(sema::Entity const* entity) {
    auto* parent = entity->parent();
    while (true) {
        if (isa<sema::FileScope>(parent)) return false;
        if (isa<sema::GlobalScope>(parent)) return false;
        if (isa<sema::Library>(parent)) return true;
        parent = parent->parent();
    }
    SC_UNREACHABLE();
}

static utl::small_vector<sema::NativeLibrary const*> topsortLibraries(
    std::span<sema::NativeLibrary const* const> libs) {
    auto result = libs | ToSmallVector<>;
    utl::topsort(result.begin(), result.end(), [](auto* lib) {
        return lib->dependencies() | Filter<sema::NativeLibrary>;
    });
    return result;
}

void irgen::generateIR(ir::Context& ctx, ir::Module& mod, ast::ASTNode const&,
                       sema::SymbolTable const& sym,
                       sema::AnalysisResult const& analysisResult,
                       Config config) {
    TypeMap typeMap(ctx);
    GlobalMap globalMap;
    /// We import libraries in topsort order because there may be dependencies
    /// between the libraries
    auto libs = topsortLibraries(sym.importedLibs() |
                                 Filter<sema::NativeLibrary> | ToSmallVector<>);
    for (auto* lib: libs) {
        importLibrary(ctx, mod, *lib, typeMap, globalMap, config.nameMangler);
    }
    for (auto* semaType: analysisResult.structDependencyOrder) {
        generateType(semaType, ctx, mod, typeMap, config.nameMangler);
    }
    /// We generate code for all compiler generated function of public types
    utl::small_vector<sema::Function const*> generatedFunctions;
    for (auto* type: sym.structTypes() | filter(&sema::StructType::isPublic)) {
        for (auto* F: type->entities() | Filter<sema::Function>) {
            if (F->isGenerated() && !isLibEntity(F)) {
                generatedFunctions.push_back(F);
            }
        }
    }
    auto userDefinedFunctions =
        analysisResult.globals | Filter<ast::FunctionDefinition> |
        transform([](auto* def) { return def->function(); });
    auto queue = concat(userDefinedFunctions, generatedFunctions) |
                 filter(initialDeclFilter) |
                 ranges::to<std::deque<sema::Function const*>>;
    for (auto* semaFn: queue) {
        declareFunction(*semaFn, ctx, mod, typeMap, globalMap,
                        config.nameMangler);
    }
    while (!queue.empty()) {
        auto* semaFn = queue.front();
        queue.pop_front();
        auto* irFn = globalMap(semaFn).function;
        auto* native = dyncast<ir::Function*>(irFn);
        if (!native) continue;
        generateFunction(config, { .semaFn = semaFn,
                                   .irFn = *native,
                                   .ctx = ctx,
                                   .mod = mod,
                                   .symbolTable = sym,
                                   .typeMap = typeMap,
                                   .globalMap = globalMap,
                                   .declQueue = queue });
    }
    ir::assertInvariants(ctx, mod);
    if (config.generateDebugSymbols) {
        mod.setMetadata(config.sourceFiles | transform(&SourceFile::path) |
                        ranges::to<dbi::SourceFileList>);
    }
}
