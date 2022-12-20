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
/// \details Ignores all tokens but bracket tokens, i.e. \p "(" , \p "[" , \p "{" , \p ")" , \p "]" , \p "}" .
/// \details Every matching pair of opening and closing brackets defines a syntactical scope. Every open scope must be
/// closed before the parent scope has been closed. \details E.g. \code "( ... [ ... ] ... )" \endcode is valid, but
/// \code "( ... [ ... ) ... ]" \endcode and  \code "( ... [ ... )" \endcode are not. \details If a closing bracket
/// without a directly corresponding opening bracket is encountered, all currently open scopes are considered to close
/// there. \details If that closing bracket closes a currently open scope, closing brackets for all currently open
/// nested scopes are inserted. Otherwise the closing bracket is discarded. \details After traversing the token stream,
/// closing brackets are inserted at the end for all unclosed scopes.
///
/// \details E.g. the following transformations will be applied:
/// \details \p "]" is inserted before the closing \p ")" and \p "]" is discarded as the scope opened by \p "[" has
/// already been closed: \details \code "( ... [ ... ) ... ]" -> "( ... [ ... ] ) ..." \endcode
///
/// \details \p "]" is inserted before the closing \p ")" :
/// \details \code "( ... [ ... )"       -> "( ... [ ... ] )" \endcode
///
/// \details \p "]" and \p ")" are inserted at the end:
/// \details \code "( ... [ ... "        -> "( ... [ ... ] )" \endcode
///
/// \details \p ")" is inserted before the closing \p ")" and \p "]" is discarded as the scope opened by \p "[" has
/// already been closed: \details \code " ... )"              -> " ... " \endcode
///
/// \param tokens List of tokens to process.
/// \param issueHandler Issue handler to submit errors to.
///
/// \post Every opening bracket in the token stream \p tokens has a correctly scoped matching closing bracket.
SCATHA(API) void bracketCorrection(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& issueHandler);

} // namespace scatha::parse

#endif // PARSER_BRACKETCORRECTION_H_
