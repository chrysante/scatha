#ifndef SCDIS_DISASSEMBLY_H_
#define SCDIS_DISASSEMBLY_H_

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

#include <scbinutil/OpCode.h>
#include <scbinutil/ProgramView.h>
#include <utl/hashtable.hpp>
#include <utl/projection.hpp>
#include <utl/vector.hpp>

namespace scdis {

namespace internal {
struct Disassembler;
}

///
struct Label {
    enum Type { Function, Block, String, Raw };

    Type type;
    std::string name;
};

/// Minimal set of instruction argument types
enum class ValueType {
    RegisterIndex,
    Address,
    Value8,
    Value16,
    Value32,
    Value64,
};

/// Single instruction argument
struct Value {
    ValueType type;
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
struct InstructionPointerOffset {
    explicit InstructionPointerOffset(size_t value): value(value) {}

    size_t value;

    bool operator==(InstructionPointerOffset const&) const = default;
};

/// Represents a single VM intruction.
struct Instruction {
    // Warning: The memory layout of this struct is quite bad because of many
    // padding bytes

    /// The location of this instruction in the binary
    InstructionPointerOffset ipo;

    /// The opcode of this instructions
    scbinutil::OpCode opcode;

    /// The arguments of this instruction. These can be empty depending on the
    /// opcode
    Value arg1, arg2;
};

/// Represents a global variable, i.e., a section of the data segment
struct Variable {
    Label label;
    InstructionPointerOffset ipo;
    utl::small_vector<uint8_t> data;
};

} // namespace scdis

template <>
struct std::hash<scdis::InstructionPointerOffset> {
    size_t operator()(scdis::InstructionPointerOffset ipo) const {
        return hash<size_t>{}(ipo.value);
    }
};

namespace scdis {

/// Maps instruction pointer offsets to instruction indices and vice versa
class IpoIndexMap {
public:
    /// \Returns the index of the instruction at instruction pointer offset \p
    /// ipo if possible
    std::optional<size_t> ipoToIndex(InstructionPointerOffset ipo) const;

    /// \Returns the instruction pointer offset of the instruction at index \p
    /// index
    InstructionPointerOffset indexToIpo(size_t index) const;

private:
    friend struct internal::Disassembler;

    void insertAtBack(InstructionPointerOffset ipo);

    utl::hashmap<InstructionPointerOffset, size_t> _ipoToIndex;
    std::vector<InstructionPointerOffset> _indexToIpo;
};

/// Represents a disassembled program.
class Disassembly {
public:
    Disassembly() = default;

    /// \Returns a view over the global variables in this program
    std::span<Variable const> variables() const { return _vars; }

    /// \Returns the global variable at \p index
    Variable const& variable(size_t index) const { return _vars[index]; }

    /// \Returns a view over the instructions in this program
    std::span<Instruction const> instructions() const { return _insts; }

    /// \Returns the instruction at \p index
    Instruction const& instruction(size_t index) const { return _insts[index]; }

    /// \Returns `true` if instructions are empty
    bool empty() const { return _insts.empty(); }

    ///
    IpoIndexMap const& indexMap() const { return _indexMap; }

    /// \Returns the label of the instruction at instruction index \p index
    Label const* findLabel(size_t index) const {
        auto itr = _instLabels.find(index);
        return itr != _instLabels.end() ? &itr->second : nullptr;
    }

    /// \overload
    Label const* findLabel(InstructionPointerOffset ipo) const {
        auto index = indexMap().ipoToIndex(ipo);
        return index ? findLabel(*index) : nullptr;
    }

    ///
    scbinutil::FFIDecl const* findFfiByIndex(size_t index) const {
        auto itr = _ffiset.find(index);
        return itr != _ffiset.end() ? std::to_address(itr) : nullptr;
    }

private:
    friend struct internal::Disassembler;

    std::vector<Variable> _vars;
    std::vector<Instruction> _insts;
    // Maps instruction indices to labels
    utl::hashmap<size_t, Label> _instLabels;
    IpoIndexMap _indexMap;
    template <typename T, auto Mem>
    using ProjectionHashset = utl::hashset<T, utl::projection_hash<T, Mem>,
                                           utl::projection_equal_to<T, Mem>>;
    ProjectionHashset<scbinutil::FFIDecl, &scbinutil::FFIDecl::index> _ffiset;
};

} // namespace scdis

#endif // SCDIS_DISASSEMBLY_H_
