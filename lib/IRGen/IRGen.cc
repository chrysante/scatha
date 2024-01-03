#include "IRGen/IRGen.h"

#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "Common/DebugInfo.h"
#include "IR/CFG.h"
#include "IR/Context.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/FunctionGeneration.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/Maps.h"
#include "IRGen/MetaData.h"
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
    sema::Scope const& scope,
    TypeMap& typeMap,
    FunctionMap& functionMap,
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
                               makeStructMetadata(&semaType));
            },
            [&](sema::Function const& semaFn) {
                std::string name = nameMangler(*entity);
                functionMap.insert(&semaFn,
                                   cast<ir::Function*>(get(IRObjectMap, name)),
                                   makeFunctionMetadata(&semaFn));
            },
            [&]([[maybe_unused]] sema::Entity const& entity) {}
        }; // clang-format on
    }
    for (auto* child: scope.children()) {
        mapLibSymbols(*child,
                      typeMap,
                      functionMap,
                      nameMangler,
                      IRStructMap,
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
                 }) |
                 ToSmallVector<>;
    if (fatal.empty()) {
        return;
    }
    std::cout << "Failed to parse library \"" << libName << "\":\n";
    for (auto& issue: fatal) {
        ir::print(issue);
    }
    SC_ABORT();
}

static void importLibrary(ir::Context& ctx,
                          ir::Module& mod,
                          sema::NativeLibrary const& lib,
                          TypeMap& typeMap,
                          FunctionMap& functionMap,
                          sema::NameMangler const& nameMangler) {
    std::fstream file(lib.codeFile());
    SC_RELASSERT(file,
                 utl::strcat("Failed to open file ", lib.codeFile()).c_str());
    std::stringstream sstr;
    sstr << file.rdbuf();
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
    auto parseIssues = ir::parseTo(std::move(sstr).str(),
                                   ctx,
                                   mod,
                                   { .typeParseCallback = typeCallback,
                                     .objectParseCallback = objCallback });
    checkParserIssues(parseIssues, lib.name());
    mapLibSymbols(lib,
                  typeMap,
                  functionMap,
                  nameMangler,
                  IRStructMap,
                  IRObjectMap);
}

void irgen::generateIR(ir::Context& ctx,
                       ir::Module& mod,
                       ast::ASTNode const&,
                       sema::SymbolTable const& sym,
                       sema::AnalysisResult const& analysisResult,
                       Config config) {
    TypeMap typeMap(ctx);
    FunctionMap functionMap;
    for (auto* lib: sym.importedLibs() | Filter<sema::NativeLibrary>) {
        importLibrary(ctx, mod, *lib, typeMap, functionMap, config.nameMangler);
    }
    for (auto* semaType: analysisResult.structDependencyOrder) {
        generateType(semaType, ctx, mod, typeMap, config.nameMangler);
    }
    auto queue = analysisResult.functions |
                 transform([](auto* def) { return def->function(); }) |
                 filter([](auto* fn) {
                     return fn->binaryVisibility() ==
                            sema::BinaryVisibility::Export;
                 }) |
                 ranges::to<std::deque<sema::Function const*>>;
    for (auto* semaFn: queue) {
        declareFunction(semaFn,
                        ctx,
                        mod,
                        typeMap,
                        functionMap,
                        config.nameMangler);
    }
    while (!queue.empty()) {
        auto* semaFn = queue.front();
        queue.pop_front();
        auto* irFn = functionMap(semaFn);
        auto* native = dyncast<ir::Function*>(irFn);
        if (!native) continue;
        generateFunction(config,
                         { .semaFn = *semaFn,
                           .irFn = *native,
                           .ctx = ctx,
                           .mod = mod,
                           .symbolTable = sym,
                           .typeMap = typeMap,
                           .functionMap = functionMap,
                           .declQueue = queue });
    }
    ir::assertInvariants(ctx, mod);
    if (config.generateDebugSymbols) {
        mod.setMetadata(config.sourceFiles | transform(&SourceFile::path) |
                        ranges::to<dbi::SourceFileList>);
    }
}
