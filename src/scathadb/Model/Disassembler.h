#ifndef SDB_MODEL_DISASSEMBLER_H_
#define SDB_MODEL_DISASSEMBLER_H_

#include <cassert>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <scbinutil/OpCode.h>
#include <scbinutil/ProgramView.h>
#include <svm/Fwd.h>
#include <utl/hashtable.hpp>

namespace sdb {

class Disassembly;
class SourceDebugInfo;

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
Value makeAddress(uint8_t baseRegIdx, uint8_t offsetRegIdx,
                  uint8_t offsetFactor, uint8_t offsetTerm);
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
    scbinutil::OpCode opcode;

    /// The arguments of this instruction. These can be empty depending on the
    /// opcode
    Value arg1, arg2;

    /// The label of this instruction. Empty means this instructions is
    /// unlabelled
    std::string label;
};

/// Convert the instruction \p inst to a string. If \p disasm is non-null it is
/// used to print prettier labels
std::string toString(Instruction inst, Disassembly const* disasm = nullptr,
                     svm::VirtualMachine const* vm = nullptr);

/// Print \p inst to \p ostream
std::ostream& operator<<(std::ostream& ostream, Instruction inst);

/// Disassemble the program \p program
/// Disassembling a program recomputes as much structure as possible to enable
/// debugging
Disassembly disassemble(std::span<uint8_t const> program,
                        SourceDebugInfo const& debugInfo);

/// Represents a disassembled program.
class Disassembly {
public:
    Disassembly() = default;

    /// \Returns the instruction at binary offset \p offset if there is an
    /// instruction at that offset. Otherwise returns null
    Instruction const* offsetToInstruction(size_t offset) const {
        if (auto index = offsetToIndex(offset)) {
            return &insts[*index];
        }
        return nullptr;
    }

    /// \Returns the index of the instruction at binary offset \p offset if
    /// possible
    std::optional<size_t> offsetToIndex(size_t offset) const {
        auto itr = offsetToIndexMap.find(offset);
        if (itr != offsetToIndexMap.end()) {
            return itr->second;
        }
        return std::nullopt;
    }

    /// \Returns the binary offset of the instruction at index \p index
    size_t indexToOffset(size_t index) const {
        // TODO: Return std::optional here
        assert(index < indexToOffsetMap.size());
        return indexToOffsetMap[index];
    }

    size_t sourceLineToOffset([[maybe_unused]] size_t line) const {
        assert(false);
        return ~size_t{ 0 };
    }

    /// \Returns a view over the instructions in this program
    std::span<Instruction const> instructions() const { return insts; }

    ///
    Instruction& instruction(size_t index) { return insts[index]; }

    ///
    Instruction const& instruction(size_t index) const { return insts[index]; }

    /// \Returns `true` if instructions are empty
    bool empty() const { return insts.empty(); }

private:
    friend Disassembly sdb::disassemble(std::span<uint8_t const>,
                                        SourceDebugInfo const&);

    std::vector<Instruction> insts;
    /// Maps binary offsets to instruction indices
    utl::hashmap<size_t, size_t> offsetToIndexMap;

    /// Maps instruction indices to binary offsets
    std::vector<size_t> indexToOffsetMap;
};

} // namespace sdb

#endif // SDB_MODEL_DISASSEMBLER_H_
