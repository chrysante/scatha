#ifndef SCATHA_RUNTIME_PROGRAM_H_
#define SCATHA_RUNTIME_PROGRAM_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace scatha {

class Compiler;

///
class Program {
public:
    /// \Returns the binary address of function \p name
    std::optional<size_t> getAddress(std::string name) const;

    /// \Returns a pointer to the programs binary
    uint8_t const* data() const { return _data.data(); }

private:
    friend class Compiler;
    std::vector<uint8_t> _data;
    std::unordered_map<std::string, size_t> _sym;
};

} // namespace scatha

#endif // SCATHA_RUNTIME_PROGRAM_H_
