#ifndef SCATHA_COMMON_ESCAPESEQUENCE_H_
#define SCATHA_COMMON_ESCAPESEQUENCE_H_

#include <optional>

namespace scatha {

/// Convert \p c to its corresponding escape sequence if possible
std::optional<char> toEscapeSequence(char c);

/// Convert the escape sequence \p seq to its corresponding charactar
std::optional<char> fromEscapeSequence(char seq);

} // namespace scatha

#endif // SCATHA_COMMON_ESCAPESEQUENCE_H_
