#ifndef SCATHA_RUNTIME_PROGRAM_H_
#define SCATHA_RUNTIME_PROGRAM_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <scatha/Sema/SymbolTable.h>

namespace scatha {

class Compiler;

///
class Program {
public:
    /// \Returns a pointer to the programs binary
    uint8_t const* data() const { return _data.data(); }

    /// \Returns the programs symbol table
    sema::SymbolTable const& symbolTable() const { return _sym; }

    /// \Returns the binary address of function \p name
    std::optional<size_t> getAddress(std::string name) const;

private:
    friend class Compiler;
    std::vector<uint8_t> _data;
    sema::SymbolTable _sym;
    std::unordered_map<std::string, size_t> _binsym;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_PROGRAM_H_
