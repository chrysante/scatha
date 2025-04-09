#include "IRGen/LibImport.h"

#include <iostream>
#include <span>

#include <svm/Builtin.h>
#include <utl/graph.hpp>
#include <utl/vector.hpp>

#include "Common/FileHandling.h"
#include "Common/Ranges.h"
#include "IR/CFG.h"
#include "IR/IRParser.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/LoweringContext.h"
#include "Invocation/TargetNames.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;

namespace {

/// Performs a DFS over a library scope and adds entries for all structs and
/// functions to the type map and function map
struct MapCtx {
    MapCtx(ImportMap const& importMap, LoweringContext& lctx):
        importMap(importMap), lctx(lctx) {}

    ImportMap const& importMap;
    LoweringContext& lctx;
    utl::small_vector<sema::RecordType const*> metadataDeferQueue;

    void mapScope(sema::Scope const& scope);

    void mapSymbol(sema::RecordType const&);
    void mapSymbol(sema::Function const&);
    void mapSymbol(sema::Variable const&);
    void mapSymbol(sema::Entity const&) {}
};

} // namespace

void MapCtx::mapScope(sema::Scope const& scope) {
    for (auto* entity: scope.entities()) {
        visit(*entity, [&](auto& entity) { mapSymbol(entity); });
    }
    for (auto* child: scope.children()) {
        mapScope(*child);
    }
}

void MapCtx::mapSymbol(sema::RecordType const& type) {
    std::string name = lctx.config.nameMangler(type);
    lctx.typeMap.insert(&type, importMap.get<ir::StructType>(name));
    metadataDeferQueue.push_back(&type);
}

void MapCtx::mapSymbol(sema::Function const& semaFn) {
    if (semaFn.isAbstract()) {
        return;
    }
    std::string name = lctx.config.nameMangler(semaFn);
    auto* irFn = importMap.get<ir::Function>(name);
    lctx.globalMap.insert(&semaFn, { irFn, computeCallingConvention(semaFn) });
}

void MapCtx::mapSymbol(sema::Variable const& semaVar) {
    if (!semaVar.isStatic()) {
        return;
    }
    std::string name = lctx.config.nameMangler(semaVar);
    auto* irVar = importMap.get<ir::GlobalVariable>(name);
    auto* initGuard = [&]() -> ir::GlobalVariable* {
        if (!isa<ir::UndefValue>(irVar->initializer())) {
            return nullptr;
        }
        return importMap.get<ir::GlobalVariable>(name, ".init");
    }();
    auto* getter = importMap.get<ir::Function>(name, ".getter");
    GlobalVarMetadata metadata{ .var = irVar,
                                .varInit = initGuard,
                                .getter = getter };
    lctx.globalMap.insert(&semaVar, metadata);
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
    std::cerr << "Failed to parse library \"" << libName << "\":\n";
    for (auto& issue: fatal) {
        ir::print(issue);
    }
    SC_ABORT();
}

static void importLibrary(sema::NativeLibrary const& lib, ImportMap& importMap,
                          LoweringContext& lctx) {
    auto archive = Archive::Open(lib.path());
    SC_RELASSERT(archive, "Failed to open library file");
    auto code = archive->openTextFile(TargetNames::ObjectCodeName);
    SC_RELASSERT(code, "Failed to open object code file");
    auto typeCallback = [&](ir::StructType& type, ir::DeclToken& declToken) {
        if (!importMap.insert(&type)) {
            declToken.ignore();
        }
    };
    auto objCallback = [&](ir::Global& object, ir::DeclToken&) {
        if (auto* irFn = dyncast<ir::Function*>(&object)) {
            irFn->setVisibility(ir::Visibility::Internal);
        }
        importMap.insert(&object);
    };
    auto parseIssues = ir::parseTo(*code, lctx.ctx, lctx.mod,
                                   { .typeParseCallback = typeCallback,
                                     .objectParseCallback = objCallback,
                                     .assertInvariants = false });
    checkParserIssues(parseIssues, lib.path().string());
    MapCtx mapCtx(importMap, lctx);
    mapCtx.mapScope(lib);
    /// We defer generation of type metadata because to generate vtables all
    /// functions must be declared in the global map
    for (auto* type: mapCtx.metadataDeferQueue) {
        lctx.typeMap.setMetadata(type, makeRecordMetadataImport(type, importMap,
                                                                lctx));
    }
}

static utl::small_vector<sema::NativeLibrary const*> topsortLibraries(
    std::span<sema::NativeLibrary const* const> libs) {
    auto result = libs | ToSmallVector<>;
    utl::topsort(result.begin(), result.end(), [](auto* lib) {
        return lib->dependencies() | Filter<sema::NativeLibrary>;
    });
    return result;
}

static constexpr auto BuiltinPrefix = "__builtin_";

static svm::Builtin nameToBuiltin(std::string_view name) {
    static utl::hashmap<std::string_view, svm::Builtin> const map = {
#define SVM_BUILTIN_DEF(Name, ...) { #Name, svm::Builtin::Name },
#include <svm/Builtin.def.h>
    };
    auto itr = map.find(name);
    SC_RELASSERT(itr != map.end(), "Unknown builtin name");
    return itr->second;
}

static void declareBuiltinFunction(LoweringContext& lctx,
                                   ir::ForeignFunction& F) {
    SC_EXPECT(F.name().starts_with(BuiltinPrefix));
    auto name = F.name().substr(std::strlen(BuiltinPrefix));
    svm::Builtin key = nameToBuiltin(name);
    auto* semaFn = lctx.symbolTable.builtinFunction((size_t)key);
    lctx.globalMap.insert(semaFn, { &F, computeCallingConvention(*semaFn) });
}

static void uniqueGlobals(LoweringContext& lctx) {
    utl::hashmap<std::string, ir::Global*> map;
    utl::small_vector<ir::Global*> toErase;
    auto impl = [&](ir::Global& global) {
        auto [itr, success] =
            map.insert({ std::string(global.name()), &global });
        if (success) {
            if (global.name().starts_with(BuiltinPrefix)) {
                declareBuiltinFunction(lctx,
                                       cast<ir::ForeignFunction&>(global));
            }
            return;
        }
        auto* existing = itr->second;
        SC_RELASSERT(existing->nodeType() == global.nodeType(), "");
        global.replaceAllUsesWith(existing);
        toErase.push_back(&global);
    };
    for (auto& global: lctx.mod.globals()) {
        impl(global);
    }
    for (auto& F: lctx.mod) {
        impl(F);
    }
    for (auto* global: toErase) {
        lctx.mod.erase(global);
    }
}

void irgen::importLibraries(sema::SymbolTable const& sym,
                            LoweringContext& lctx) {
    /// We import libraries in topsort order because there may be dependencies
    /// between the libraries
    auto libs = topsortLibraries(sym.importedLibs() |
                                 Filter<sema::NativeLibrary> | ToSmallVector<>);
    ImportMap importMap;
    for (auto* lib: libs) {
        importLibrary(*lib, importMap, lctx);
    }
    uniqueGlobals(lctx);
}
