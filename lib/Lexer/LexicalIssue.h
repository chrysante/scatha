#pragma once

#ifndef SCATHA_LEXER_LEXERERROR_H_
#define SCATHA_LEXER_LEXERERROR_H_

#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"

namespace scatha::lex {

class SCATHA(API) LexicalIssue: public ProgramIssue {
  protected:
    LexicalIssue(Token const &, std::string const &message);
};

class SCATHA(API) UnexpectedID: public LexicalIssue {
  public:
    explicit UnexpectedID(Token const &);
};

class SCATHA(API) InvalidNumericLiteral: public LexicalIssue {
  public:
    explicit InvalidNumericLiteral(Token const &);
};

class SCATHA(API) UnterminatedMultiLineComment: public LexicalIssue {
  public:
    explicit UnterminatedMultiLineComment(Token const &);
};

class SCATHA(API) UnterminatedStringLiteral: public LexicalIssue {
  public:
    explicit UnterminatedStringLiteral(Token const &);
};

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXERERROR_H_
