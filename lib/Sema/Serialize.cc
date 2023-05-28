#include "Sema/Serialize.h"

#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct SerializeContext {
    explicit SerializeContext(SymbolTable const& sym, std::vector<char>& dest):
        sym(sym), dest(dest) {}

    SymbolTable const& sym;
    std::vector<char>& dest;
};

} // namespace

std::vector<char> sema::serialize(SymbolTable const& symbolTable) {}

SymbolTable sema::deserialize(std::span<char const> data) {}
