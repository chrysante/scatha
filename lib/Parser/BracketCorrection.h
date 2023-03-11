#ifndef PARSER_BRACKETCORRECTION_H_
#define PARSER_BRACKETCORRECTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"

namespace scatha::parse {

/// \brief Handles bracket mismatches in the token stream \p tokens by erasing and inserting bracket tokens and
/// submitting errors to \p issueHandler
///
/// \details Bracket correcion is the first parsing step.
/// \details Ignores all tokens but bracket tokens, i.e. `(` , `[` , `{` , `)` , `]` , `}` .
/// \details Every matching pair of opening and closing brackets defines a syntactical scope. Every open scope must be
/// closed before the parent scope has been closed. \details E.g. `"( ... [ ... ] ... )"` is valid, but
/// `"( ... [ ... ) ... ]"` and `"( ... [ ... )"` are not.
/// \details If a closing bracket without a directly corresponding opening bracket is encountered, all currently open
/// scopes are considered to close there.
/// If that closing bracket closes a currently open scope, closing brackets for all currently open
/// nested scopes are inserted. Otherwise the closing bracket is discarded.
/// After traversing the token stream,
/// closing brackets are inserted at the end for all unclosed scopes.
///
/// E.g. the following transformations will be applied:
/// `]` is inserted before the closing `)` and `]` is discarded as the scope opened by `[` has
/// already been closed: `"( ... [ ... ) ... ]" -> "( ... [ ... ] ) ..."`
///
/// `"]"` is inserted before the closing `")"`:
/// `"( ... [ ... )"       -> "( ... [ ... ] )"`
///
/// `"]"` and `")"` are inserted at the end:
/// `"( ... [ ... "        -> "( ... [ ... ] )"`
///
/// `)` is inserted before the closing `)` and `]` is discarded as the scope opened by `[` has
/// already been closed: `"...)" -> "..."
///
/// \param tokens List of tokens to process.
/// \param issueHandler Issue handler to submit errors to.
///
/// \post Every opening bracket in the token stream \p tokens has a correctly scoped matching closing bracket.
SCATHA(API) void bracketCorrection(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& issueHandler);

} // namespace scatha::parse

#endif // PARSER_BRACKETCORRECTION_H_
