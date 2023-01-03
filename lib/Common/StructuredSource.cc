#include "Common/StructuredSource.h"

#include "Lexer/ExtractLines.h"

using namespace scatha;

StructuredSource::StructuredSource(std::string_view source): lines(lex::extractLines(source)) {}
