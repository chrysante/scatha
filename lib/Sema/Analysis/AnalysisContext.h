#ifndef SCATHA_SEMA_ANALYSISCONTEXT_H_
#define SCATHA_SEMA_ANALYSISCONTEXT_H_

#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Semantic analysis context
/// Owns the symbol table and has a reference to the issue handler
class SCATHA_API AnalysisContext {
public:
    AnalysisContext(SymbolTable& sym, IssueHandler& issueHandler):
        sym(&sym), iss(&issueHandler) {}

    /// \Returns the symbol table of this context
    SymbolTable& symbolTable() const& { return *sym; }

    /// \Returns the issue handler of this context
    IssueHandler& issueHandler() const { return *iss; }

private:
    SymbolTable* sym;
    IssueHandler* iss;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSISCONTEXT_H_
