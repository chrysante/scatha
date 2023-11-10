#ifndef SDB_DISASSEMBLER_H_
#define SDB_DISASSEMBLER_H_

#include <optional>
#include <string>
#include <vector>

#include <svm/OpCode.h>
#include <svm/Program.h>
#include <utl/hashtable.hpp>

namespace sdb {

class Disassembly;

///
struct Value {
    enum Type {
        RegisterIndex,
        Address,
        Value8,
        Value16,
        Value32,
        Value64,
    };

    Type type;
    uint64_t raw;
};

/// `Value` Constructors
/// @{
Value makeRegisterIndex(size_t index);
Value makeAddress(uint8_t baseRegIdx,
                  uint8_t offsetRegIdx,
                  uint8_t offsetFactor,
                  uint8_t offsetTerm);
Value makeAddress(uint32_t value);
Value makeValue8(uint64_t value);
Value makeValue16(uint64_t value);
Value makeValue32(uint64_t value);
Value makeValue64(uint64_t value);
/// @}

///
std::string toString(Value value);

///
std::ostream& operator<<(std::ostream& ostream, Value value);

///
struct Instruction {
    svm::OpCode opcode;
    Value arg1, arg2;
};

///
std::string toString(Instruction inst);

///
std::ostream& operator<<(std::ostream& ostream, Instruction inst);

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
