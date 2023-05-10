#ifndef SCATHA_COMMON_ESCAPESEQUENCE_H_
#define SCATHA_COMMON_ESCAPESEQUENCE_H_

#include <iosfwd>
#include <optional>
#include <string_view>

namespace scatha {

/// Convert \p c to its corresponding escape sequence if possible
std::optional<char> toEscapeSequence(char c);

/// Convert the escape sequence \p seq to its corresponding character
std::optional<char> fromEscapeSequence(char seq);

/// Print string \p text to \p ostream with escape sequences replaced by `\` +
/// corresponding character
void printWithEscapeSeqs(std::ostream& ostream, std::string_view text);

} // namespace scatha

#endif // SCATHA_COMMON_ESCAPESEQUENCE_H_
