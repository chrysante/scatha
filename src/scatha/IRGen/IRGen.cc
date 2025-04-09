#include "IRGen/IRGen.h"

#include <fstream>
#include <queue>
#include <vector>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>

#include "AST/AST.h"
#include "Common/DebugInfo.h"
#include "IR/CFG/Function.h"
#include "IR/CFG/GlobalVariable.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "IR/Type.h"
#include "IR/Validate.h"
#include "IRGen/FunctionGeneration.h"
#include "IRGen/GlobalDecls.h"
#include "IRGen/LibImport.h"
#include "IRGen/Maps.h"
#include "IRGen/Metadata.h"
#include "Sema/AnalysisResult.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace irgen;
using namespace ranges::views;

/// \Returns `true` for all functions that are generated unconditionally, i.e.
/// even if they are not called by other functions
static bool initialDeclFilter(sema::Function const* function) {
    return !function->isAbstract() &&
           mapVisibility(function) == ir::Visibility::External;
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

static void declareInitialFunctions(sema::AnalysisResult const& analysisResult,
                                    LoweringContext& lctx) {
    /// We generate code for all compiler generated functions of public types
    utl::small_vector<sema::Function const*> generatedFunctions;
    for (auto* type: lctx.symbolTable.recordTypes() | Filter<sema::StructType> |
                         filter(&sema::StructType::isPublic))
    {
        for (auto* F: type->entities() | Filter<sema::Function>) {
            if (F->isGenerated() && !isLibEntity(F)) {
                generatedFunctions.push_back(F);
            }
        }
    }
    /// We initially declare all public user defined functions
    auto globalFunctions = analysisResult.globals |
                           Filter<ast::FunctionDefinition> |
                           transform([](auto* def) { return def->function(); });
    auto all = concat(globalFunctions, generatedFunctions) |
               filter(initialDeclFilter);
    for (auto* semaFn: all) {
        lctx.declQueue.push_back(semaFn);
        lctx.lowered.insert(semaFn);
    }
}

void irgen::generateIR(ir::Context& ctx, ir::Module& mod, ast::ASTNode const&,
                       sema::SymbolTable const& sym,
                       sema::AnalysisResult const& analysisResult,
                       Config config) {
    LoweringContext lctx(ctx, mod, sym, config);
    importLibraries(sym, lctx);
    declareInitialFunctions(analysisResult, lctx);
    for (auto* semaType: analysisResult.recordDependencyOrder) {
        generateType(semaType, lctx);
    }
    /// Declare all functions that are initially in the decl queue
    for (auto* semaFn: lctx.declQueue) {
        /// `getFunction` lazily declares the function
        (void)getFunction(*semaFn, lctx);
    }
    /// And we generate code for all public global variables
    auto globalVariables = analysisResult.globals |
                           Filter<ast::VariableDeclaration> |
                           transform([](auto* def) { return def->variable(); });
    for (auto* var: globalVariables) {
        generateGlobalVariable(*var, lctx);
    }
    while (!lctx.declQueue.empty()) {
        auto* semaFn = lctx.declQueue.front();
        lctx.declQueue.pop_front();
        auto* irFn = lctx.globalMap(semaFn).function;
        auto* native = dyncast<ir::Function*>(irFn);
        if (!native) continue;
        generateFunction(semaFn, *native, lctx);
    }
    ir::assertInvariants(ctx, mod);
    if (config.generateDebugSymbols) {
        mod.setMetadata(config.sourceFiles | transform(&SourceFile::path) |
                        ranges::to<dbi::SourceFileList>);
    }
}
