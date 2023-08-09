#include "Opt/Pipeline/PipelineError.h"

#include <utl/strcat.hpp>

using namespace scatha;
using namespace opt;

PipelineError::PipelineError(size_t column,
                             size_t line,
                             std::string_view message):
    std::runtime_error(
        utl::strcat("Error at L:", line, " C:", column, ": \"", message, "\"")),
    column(column),
    line(line) {}
