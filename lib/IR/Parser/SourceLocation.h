#ifndef SCATHA_IR_PARSER_SOURCELOCATION_H_
#define SCATHA_IR_PARSER_SOURCELOCATION_H_

#include "Basic/Basic.h"

namespace scatha::ir {

struct SourceLocation {
    size_t line = 0, column = 0;
};

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_SOURCELOCATION_H_
