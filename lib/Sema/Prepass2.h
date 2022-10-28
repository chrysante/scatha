#ifndef SCATHA_SEMA_PREPASS2_H_
#define SCATHA_SEMA_PREPASS2_H_

#include <optional>
#include <tuple>

#include <utl/vector.hpp>

#include "AST/AST.h"
#include "Basic/Basic.h"
#include "Issue/IssueHandler.h"
#include "Sema/ObjectType.h"
#include "Sema/SymbolTable.h"

namespace scatha::sema {

struct DependencyGraphNode {
    SymbolID symbolID;
    SymbolCategory category;
    ast::AbstractSyntaxTree* astNode = nullptr;
    utl::small_vector<u16> dependencies;
};

/// In prepass we declare (but not instantiate) all non-local names in the translation unit, including nested structs and member variables and functions.
/// With that we build a dependency graph of the declarations in the program.
/// \param astRoot Root node of the abstract syntax tree.
/// \param issueHandler Handler to write issues to.
/// \returns Generated \p SymbolTable and vector of \p DependencyGraphNodes which can
///  be topologically sorted and used to traverse the AST in proper depedency order
///
/// \details A _vertex_ in the dependency graph is a function declaration, a struct declaration or non-local variable declaration.
/// A vertex \p x _strongly depends_ on another vertex \p y if \p y must be instantiated for the instantiation of \p x
/// A vertex \p x _weakly depends_ on another vertex \p y if \p y must be declared for the declaration of \p x
SCATHA(API) std::tuple<SymbolTable,
                       utl::vector<DependencyGraphNode>> prepass2(ast::AbstractSyntaxTree& astRoot,
                                                                  issue::IssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_PREPASS2_H_

