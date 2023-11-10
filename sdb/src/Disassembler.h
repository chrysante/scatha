#ifndef SDB_DISASSEMBLER_H_
#define SDB_DISASSEMBLER_H_

#include <optional>
#include <vector>

#include <svm/OpCode.h>
#include <svm/Program.h>
#include <utl/hashtable.hpp>

namespace sdb {

class Disassembly;

///
struct Instruction {
    svm::OpCode opcode;
};

///
Disassembly disassemble(uint8_t const* program);

///
class Disassembly {
public:
    Disassembly() = default;

    ///
    Instruction const* instructionAt(size_t offset) const {
        if (auto index = instIndexAt(offset)) {
            return &insts[*index];
        }
        return nullptr;
    }

    ///
    std::optional<size_t> instIndexAt(size_t offset) const {
        auto itr = offsetIndexMap.find(offset);
        if (itr != offsetIndexMap.end()) {
            return itr->second;
        }
        return std::nullopt;
    }

    std::span<Instruction const> instructions() const { return insts; }

private:
    friend Disassembly disassemble(uint8_t const*);

    std::vector<Instruction> insts;
    /// Maps binary offsets to instruction indices
    utl::hashmap<size_t, size_t> offsetIndexMap;
};

} // namespace sdb

#endif // SDB_DISASSEMBLER_H_
