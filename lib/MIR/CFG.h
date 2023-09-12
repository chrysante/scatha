#ifndef SCATHA_MIR_CFG_H_
#define SCATHA_MIR_CFG_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "Common/List.h"
#include "Common/UniquePtr.h"
#include "MIR/Fwd.h"
#include "MIR/Register.h" // Maybe we can remove this later
#include "MIR/RegisterSet.h"
#include "MIR/Value.h"

namespace scatha::mir {

template <typename T>
concept InstructionData =
    sizeof(T) <= sizeof(uint64_t) && std::is_trivially_copyable_v<T>;

/// Represents an instruction.
class Instruction:
    public ListNode<Instruction>,
    public ParentedNode<BasicBlock> {
    template <typename T>
    static uint64_t convInstData(T data) {
        uint64_t res = 0;
        std::memcpy(&res, &data, sizeof(T));
        return res;
    }

    static auto destsImpl(auto* self) {
        return ranges::views::generate([dest = self->dest()]() mutable {
                   auto* result = dest;
                   dest = dest->next();
                   return result;
               }) |
               ranges::views::take(self->numDests());
    }

public:
    template <InstructionData T = uint64_t>
    explicit Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands,
                         T instData,
                         size_t width):
        Instruction(opcode,
                    dest,
                    std::move(operands),
                    convInstData(instData),
                    width) {}

    explicit Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands = {},
                         uint64_t instData = 0,
                         size_t width = 8);

    /// Set the register that this instruction defines to \p dest
    void setDest(Register* dest, size_t numDests);

    /// \overload for `numDests = dest ? 1 : 0`
    void setDest(Register* dest);

    /// This function is used to replace registers as we lower. It does not
    /// modify `numDests`. It only updates the first destination register and
    /// the remaining registers then are the registers following the new first
    /// dest
    void setFirstDest(Register* firstDest);

    /// Set the destination register to `nullptr` and `numDests` to 1
    void clearDest();

    /// Set the operands this instruction uses
    void setOperands(utl::small_vector<Value*> operands);

    /// Set the operand at index \p index to \p operand
    /// \pre \p index must be a valid operand index. Internal operand list will
    /// not be resized.
    void setOperandAt(size_t index, Value* operand);

    /// Remove all operands from this instruction
    /// Also clears the internal list of operands (does not set operands
    /// `nullptr`)
    void clearOperands();

    /// Replace all occurences of operand \p old by \p repl
    void replaceOperand(Value* old, Value* repl);

    /// \Returns The inst code of this instruction
    InstCode instcode() const { return oc; }

    /// \Returns The number of bytes this instruction defines
    size_t bytewidth() const { return _width; }

    /// \Returns The number of bits this instruction defines
    size_t bitwidth() const { return 8 * bytewidth(); }

    /// \Returns The register this instruction defines
    Register* dest() { return _dest; }

    /// \overload
    Register const* dest() const { return _dest; }

    /// \returns the destination register if this instructions defines exactly
    /// one register. Otherwise returns `nullptr`
    Register* singleDest() {
        return const_cast<Register*>(std::as_const(*this).singleDest());
    }

    /// \overload
    Register const* singleDest() const;

    /// View over all destination registers. This is usually just on register
    /// but might be more, for example on `ret` instructions
    auto destRegisters() { return destsImpl(this); }

    /// \overload
    auto destRegisters() const { return destsImpl(this); }

    /// Only applicable for `call` instructions in SSA form.
    /// Hopefully we can generalize this in the future to instructions
    /// which may have multiple (consecutive) dest registers.
    /// This would also be useful for vector instructions.
    size_t numDests() const { return _numDests; }

    /// \Returns The index of this instruction. Only valid if function has been
    /// linearized.
    size_t index() const { return _index; }

    /// \Returns The operand at index \p index
    /// Possibly cast to derived value type `V`
    template <typename V = Value>
    V* operandAt(size_t index) {
        return const_cast<V*>(
            static_cast<Instruction const*>(this)->operandAt<V>(index));
    }

    /// \overload
    template <typename V = Value>
    V const* operandAt(size_t index) const {
        return cast_or_null<V const*>(ops[index]);
    }

    /// \Returns A view of pointers to the operands of this instruction
    std::span<Value* const> operands() { return ops; }

    /// \overload
    std::span<Value const* const> operands() const { return ops; }

    /// Set the constant instruction data to \p data
    template <InstructionData T>
    void setInstData(T data) {
        std::memcpy(&_instData, &data, sizeof(T));
    }

    /// \Returns The constant instruction data
    uint64_t instData() const { return _instData; }

    /// \Returns The constant instruction data interpreted as type`T`
    template <InstructionData T>
    T instDataAs() const {
        alignas(T) char buffer[sizeof(T)];
        std::memcpy(&buffer, &_instData, sizeof(T));
        return reinterpret_cast<T&>(buffer);
    }

    /// Clone this instruction
    UniquePtr<Instruction> clone();

private:
    friend class Function;
    void setIndex(size_t index) { _index = utl::narrow_cast<uint32_t>(index); }

    InstCode oc; // 2 Bytes
    uint16_t _width;
    uint32_t _index = ~0u;
    Register* _dest = nullptr;
    utl::small_vector<Value*> ops;
    uint64_t _instData;
    uint16_t _numDests = 0;
};

} // namespace scatha::mir

namespace scatha::mir {

/// Represents a constant
/// Constants are untyped and 64 bit wide (represented as 64 bit uint)
class Constant: public Value {
public:
    Constant(uint64_t value, size_t width):
        Value(NodeType::Constant), val(value), _width(width) {}

    /// \Returns the value of this constant
    uint64_t value() const { return val; }

    /// \Returns The size of this constant
    size_t bytewidth() const { return _width; }

    /// \Returns The size of this constant in bits
    size_t bitwidth() const { return 8 * bytewidth(); }

private:
    uint64_t val = 0;
    size_t _width = 0;
};

/// Represents an undefined value
class UndefValue: public Value {
public:
    UndefValue(): Value(NodeType::UndefValue) {}
};

/// Represents a basic block
class BasicBlock:
    public ListNodeOverride<BasicBlock, Value>,
    public ParentedNode<Function>,
    public GraphNode<void, BasicBlock, GraphKind::Directed>,
    public CFGList<BasicBlock, Instruction> {
    using ListBase = CFGList<BasicBlock, Instruction>;

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    explicit BasicBlock(ir::BasicBlock const* irBB);

    /// \Returns The name of this basic block
    std::string_view name() const { return _name; }

    /// Mark register \p reg as live-in
    /// If `count > 1` also the `count - 1` registers after \p reg are marked as
    /// live in
    void addLiveIn(Register* reg, size_t count = 1) {
        addLiveImpl(_liveIn, reg, count);
    }

    /// Unmark register \p reg is live-in
    void removeLiveIn(Register* reg, size_t count = 1) {
        removeLiveImpl(_liveIn, reg, count);
    }

    /// Mark register \p reg as live-out
    /// See `addLiveIn()` for details
    void addLiveOut(Register* reg, size_t count = 1) {
        addLiveImpl(_liveOut, reg, count);
    }

    /// Unmark register \p reg is live-out
    void removeLiveOut(Register* reg, size_t count = 1) {
        removeLiveImpl(_liveOut, reg, count);
    }

    /// \Returns `true` if register \p reg is live-in
    bool isLiveIn(Register const* reg) const { return _liveIn.contains(reg); }

    /// \Returns `true` if register \p reg is live.out
    bool isLiveOut(Register const* reg) const { return _liveOut.contains(reg); }

    /// \Returns The set of live-in registers
    utl::hashset<Register*> const& liveIn() const { return _liveIn; }

    /// Set the live-in registers
    void setLiveIn(utl::hashset<Register*> liveIn) {
        _liveIn = std::move(liveIn);
    }

    /// \Returns The set of live-out registers
    utl::hashset<Register*> const& liveOut() const { return _liveOut; }

    /// Set the live-out registers
    void setLiveOut(utl::hashset<Register*> liveOut) {
        _liveOut = std::move(liveOut);
    }

    /// \Returns `true` if this is the entry basic block
    bool isEntry() const;

    /// \Returns the corresponding IR basic block this block is derived from, or
    /// `nullptr` if nonesuch exists.
    ir::BasicBlock const* irBasicBlock() const { return irBB; }

private:
    friend class Function;
    friend class CFGList<BasicBlock, Instruction>;

    void insertCallback(Instruction& inst);
    void eraseCallback(Instruction const& inst);

    void addLiveImpl(utl::hashset<Register*>& set, Register* reg, size_t count);
    void removeLiveImpl(utl::hashset<Register*>& set,
                        Register* reg,
                        size_t count);

    std::string _name;
    utl::hashset<Register*> _liveIn, _liveOut;
    ir::BasicBlock const* irBB = nullptr;
};

/// Represents a function
class Function:
    public ListNodeOverride<Function, Value>,
    public CFGList<Function, BasicBlock> {
    using ListBase = CFGList<Function, BasicBlock>;

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    /// Construct a `mir::Function` referencing \p irFunc with \p numRegisters
    /// number of virtual registers.
    explicit Function(ir::Function const* irFunc,
                      size_t numArgRegisters,
                      size_t numReturnRegisters,
                      Visibility vis);

    /// \Returns The name of this function.
    std::string_view name() const { return _name; }

    /// \Returns The number of registers filled with arguments by the caller.
    size_t numArgumentRegisters() const { return numArgRegs; }

    /// \Returns The number of registers to be filled with the return value by
    /// the callee.
    size_t numReturnValueRegisters() const { return numRetvalRegs; }

    /// \Returns a view over the instructions in this function
    auto instructions() { return *this | ranges::views::join; }

    /// \overload
    auto instructions() const { return *this | ranges::views::join; }

    /// # SSA registers

    /// \Returns The set of SSA registers used by this function.
    RegisterSet<SSARegister>& ssaRegisters() { return ssaRegs; }

    ///  \overload
    RegisterSet<SSARegister> const& ssaRegisters() const { return ssaRegs; }

    /// SSA registers used by the arguments to this function.
    std::span<SSARegister* const> ssaArgumentRegisters() {
        return std::span<SSARegister* const>(ssaRegs.flat().data(), numArgRegs);
    }

    /// \overload
    std::span<SSARegister const* const> ssaArgumentRegisters() const {
        return std::span<SSARegister const* const>(ssaRegs.flat().data(),
                                                   numArgRegs);
    }

    /// # Virtual registers

    /// \Returns The set of virtual registers used by this function.
    RegisterSet<VirtualRegister>& virtualRegisters() { return virtRegs; }

    /// \overload
    RegisterSet<VirtualRegister> const& virtualRegisters() const {
        return virtRegs;
    }

    /// Virtual registers used by the arguments to this function.
    std::span<VirtualRegister* const> virtualArgumentRegisters() {
        return std::span<VirtualRegister* const>(virtRegs.flat().data(),
                                                 numArgRegs);
    }

    /// \overload
    std::span<VirtualRegister const* const> virtualArgumentRegisters() const {
        return std::span<VirtualRegister const* const>(virtRegs.flat().data(),
                                                       numArgRegs);
    }

    /// Virtual registers used for the return value of this function.
    std::span<VirtualRegister* const> virtualReturnValueRegisters() {
        return std::span<VirtualRegister* const>(virtRegs.flat().data(),
                                                 numRetvalRegs);
    }

    /// \overload
    std::span<VirtualRegister const* const> virtualReturnValueRegisters()
        const {
        return std::span<VirtualRegister const* const>(virtRegs.flat().data(),
                                                       numRetvalRegs);
    }

    /// # Callee registers

    /// \Returns The set of callee registers used by this function.
    RegisterSet<CalleeRegister>& calleeRegisters() { return calleeRegs; }

    ///  \overload
    RegisterSet<CalleeRegister> const& calleeRegisters() const {
        return calleeRegs;
    }

    /// # Hardware registers

    /// \Returns The set of hardware registers used by this function.
    RegisterSet<HardwareRegister>& hardwareRegisters() { return hardwareRegs; }

    ///  \overload
    RegisterSet<HardwareRegister> const& hardwareRegisters() const {
        return hardwareRegs;
    }

    /// \Returns Pointer to the entry basic block
    BasicBlock* entry() { return &front(); }

    /// \overload
    BasicBlock const* entry() const { return &front(); }

    /// \Return Pointer to the `ir::Function` corresponding to this
    /// `mir::Function`.
    ir::Function const* irFunction() const { return irFunc; }

    /// Assign indices to the instructions in this function and create a table
    /// to index
    void linearizeInstructions();

    /// \Returns The instructions with index \p index
    /// \Param index An instruction index generated by `linearizeInstructions()`
    Instruction* instructionAt(size_t index) { return instrs[index]; }

    /// \overload
    Instruction const* instructionAt(size_t index) const {
        return instrs[index];
    }

    /// \Returns The visibility of this function, i.e. `extern` or `static`
    Visibility visibility() const { return vis; }

private:
    friend class CFGList<Function, BasicBlock>;

    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    std::string _name;

    RegisterSet<SSARegister> ssaRegs;
    RegisterSet<VirtualRegister> virtRegs;
    RegisterSet<CalleeRegister> calleeRegs;
    RegisterSet<HardwareRegister> hardwareRegs;

    /// Flat array of pointers to instructions in this function. Populated by
    /// `linearizeInstructions()`.
    utl::vector<Instruction*> instrs;

    ir::Function const* irFunc = nullptr;
    size_t numArgRegs    : 20 = 0;
    size_t numRetvalRegs : 20 = 0;
    Visibility vis;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
