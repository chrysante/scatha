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

/// Converts any character in the string \p text for which an escape sequence
/// exists to its literal representation. E.g. this converts the sequence
/// `"Hello World0xA"`,
/// where `0xA` represents  the actual value of`\n`, to the literal
/// `"Hello World\n"`
/// as it would be written in source code
std::string toEscapeLiteral(std::string_view text);

/// Inverser of `toEscapeLiteral()`
std::string toEscapedValue(std::string_view text);

/// Print string \p text to \p ostream with escape sequences replaced by `\` +
/// corresponding character
void printWithEscapeSeqs(std::ostream& ostream, std::string_view text);

} // namespace scatha

#endif // SCATHA_COMMON_ESCAPESEQUENCE_H_
