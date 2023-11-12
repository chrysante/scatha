#ifndef SDB_MODEL_DISASSEMBLER_H_
#define SDB_MODEL_DISASSEMBLER_H_

#include <optional>
#include <span>
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

/// Represents a single VM intruction
struct Instruction {
    /// The opcode of this instructions
    svm::OpCode opcode;

    /// The arguments of this instruction. These can be empty depending on the
    /// opcode
    Value arg1, arg2;

    /// The ID of the label of this instruction. Zero means this instructions is
    /// unlabelled
    size_t labelID = 0;
};

/// Convert the instruction \p inst to a string. If \p disasm is non-null it is
/// used to print prettier labels
std::string toString(Instruction inst, Disassembly const* disasm = nullptr);

/// Print \p inst to \p ostream
std::ostream& operator<<(std::ostream& ostream, Instruction inst);

/// Convert the label id \p ID to a name
std::string labelName(size_t ID);

/// Disassemble the program \p program
/// Disassembling a program recomputes as much structure as possible to enable
/// debugging
Disassembly disassemble(std::span<uint8_t const> program);

/// Represents a disassembled program.
class Disassembly {
public:
    Disassembly() = default;

    /// \Returns the instruction at binary offset \p offset if there is an
    /// instruction at that offset. Otherwise returns null
    Instruction const* instructionAt(size_t offset) const {
        if (auto index = instIndexAt(offset)) {
            return &insts[*index];
        }
        return nullptr;
    }

    /// \Returns the index of the instruction at binary offset \p offset if
    /// possible
    std::optional<size_t> instIndexAt(size_t offset) const {
        auto itr = offsetIndexMap.find(offset);
        if (itr != offsetIndexMap.end()) {
            return itr->second;
        }
        return std::nullopt;
    }

    /// \Returns a view over the instructions in this program
    std::span<Instruction const> instructions() const { return insts; }

    /// \Returns `true` if instructions are empty
    bool empty() const { return insts.empty(); }

private:
    friend Disassembly sdb::disassemble(std::span<uint8_t const>);

    std::vector<Instruction> insts;
    /// Maps binary offsets to instruction indices
    utl::hashmap<size_t, size_t> offsetIndexMap;
};

} // namespace sdb

#endif // SDB_MODEL_DISASSEMBLER_H_
