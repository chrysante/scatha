#ifndef SCATHA_SEMA_SERIALIZE_H_
#define SCATHA_SEMA_SERIALIZE_H_

#include <span>
#include <vector>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

std::vector<char> serialize(SymbolTable const& symbolTable);

SymbolTable deserialize(std::span<char const> data);

} // namespace scatha::sema

#endif // SCATHA_SEMA_SERIALIZE_H_
