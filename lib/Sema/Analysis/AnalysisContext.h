#ifndef SCATHA_SEMA_ANALYSISCONTEXT_H_
#define SCATHA_SEMA_ANALYSISCONTEXT_H_

#include <utl/hashtable.hpp>

#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

/// Semantic analysis context
/// Has references to the symbol table and  the issue handler and stores some
/// indermediate analysis data
class SCATHA_API AnalysisContext {
public:
    AnalysisContext(SymbolTable& sym, IssueHandler& issueHandler):
        sym(&sym), iss(&issueHandler) {}

    /// \Returns the symbol table of this context
    SymbolTable& symbolTable() const& { return *sym; }

    /// \Returns the issue handler of this context
    IssueHandler& issueHandler() const { return *iss; }

    /// ## Cycle detection in return type deduction
    /// To deduce return types we recursively analyze functions when needed. To
    /// detect cycles we maintain a set of functions that are currently being
    /// analyzed
    /// @{
    /// Add a function to the set of functions that are currently being analyzed
    void beginAnalyzing(Function const* function) {
        currentlyAnalyzedFunctions.insert(function);
    }

    /// Remove a function to the set of functions that are currently being
    /// analyzed
    void endAnalyzing(Function const* function) {
        currentlyAnalyzedFunctions.erase(function);
        analyzedFunctions.insert(function);
    }

    /// \Returns `true` if \p function is currently being analyzed
    bool isAnalyzing(Function const* function) {
        return currentlyAnalyzedFunctions.contains(function);
    }

    /// \Returns `true` if \p function is currently being analyzed
    bool isAnalyzed(Function const* function) {
        return analyzedFunctions.contains(function);
    }
    /// @}

    /// Conveniece wrapper to emit issues
    template <typename I, typename... Args>
    void issue(auto* astNode, Args&&... args) {
        iss->push<I>(astNode,
                     &sym->currentScope(),
                     std::forward<Args>(args)...);
    }

private:
    SymbolTable* sym;
    IssueHandler* iss;
    utl::hashset<Function const*> analyzedFunctions;
    utl::hashset<Function const*> currentlyAnalyzedFunctions;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSISCONTEXT_H_
