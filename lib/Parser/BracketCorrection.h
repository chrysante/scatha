#ifndef PARSER_BRACKETCORRECTION_H_
#define PARSER_BRACKETCORRECTION_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Issue/IssueHandler.h"

namespace scatha::parse {
	
/// \brief Bracket correcion is the first parsing step.
/// \brief Handles bracket mismatches in the token stream \p tokens by erasing and inserting bracket tokens  and submitting errors to \p issueHandler
///
/// \param tokens List of tokens to process.
/// \param issueHandler Issue handler to submit errors to. 
///
/// \post Every opening bracket in the token stream \p tokens has a correctly scoped matching closing bracket.
SCATHA(API) void bracketCorrection(utl::vector<Token>& tokens, issue::SyntaxIssueHandler& issueHandler);
	
}

#endif // PARSER_BRACKETCORRECTION_H_
