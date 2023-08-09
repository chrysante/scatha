#ifndef SCATHA_OPT_PIPELINEERROR_H_
#define SCATHA_OPT_PIPELINEERROR_H_

#include <stdexcept>
#include <string>
#include <string_view>

#include "Common/Base.h"

namespace scatha::opt {

/// Base class of pipeline parsing errors
struct SCATHA_API PipelineError: std::runtime_error {
    PipelineError(size_t column, size_t line, std::string_view message);

    size_t column;
    size_t line;
};

/// Lexing error during pipeline parsing
struct SCATHA_API PipelineLexicalError: PipelineError {
    using PipelineError::PipelineError;
};

/// Syntax error during pipeline parsing
struct SCATHA_API PipelineSyntaxError: PipelineError {
    using PipelineError::PipelineError;
};

} // namespace scatha::opt

#endif // SCATHA_OPT_PIPELINEERROR_H_
