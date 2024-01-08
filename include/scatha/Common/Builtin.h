#ifndef SCATHA_COMMON_BUILTIN_H_
#define SCATHA_COMMON_BUILTIN_H_

#include <optional>
#include <string_view>

#include <scatha/Common/Base.h>

namespace scatha {

/// \Returns the index in the builtin table of the function with name \p name
std::optional<size_t> getBuiltinIndex(std::string_view name);

} // namespace scatha

#endif // SCATHA_COMMON_BUILTIN_H_
