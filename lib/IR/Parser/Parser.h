#ifndef SCATHA_IR_PARSER_PARSER_H_
#define SCATHA_IR_PARSER_PARSER_H_

#include <string_view>

#include "Basic/Basic.h"
#include "Common/Expected.h"
#include "IR/Parser/SourceLocation.h"

namespace scatha::ir {

class Context;
class Module;

class ParseError {
public:
    explicit ParseError(SourceLocation loc): _loc(loc) {}

    SourceLocation sourceLocation() const { return _loc; }

private:
    SourceLocation _loc;
};

/// Parses \p text into an IR module.
SCATHA_API Expected<Module, ParseError> parse(std::string_view text,
                                              Context& context);

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_PARSER_H_
