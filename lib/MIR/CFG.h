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

///
///
///
class Instruction:
    public ListNode<Instruction>,
    public ParentedNode<BasicBlock> {
    template <typename T>
    static uint64_t convInstData(T data) {
        uint64_t res = 0;
        std::memcpy(&res, &data, sizeof(T));
        return res;
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
                         uint64_t instData                  = 0,
                         size_t width                       = 8);

    void setDest(Register* dest);

    void setOperands(utl::small_vector<Value*> operands);

    void setOperandAt(size_t index, Value* operand);

    void clearOperands();

    InstCode instcode() const { return oc; }

    size_t bytewidth() const { return _width; }

    size_t bitwidth() const { return 8 * bytewidth(); }

    Register* dest() { return _dest; }

    Register const* dest() const { return _dest; }

    /// \Returns The index of this instruction. Only valid if function has been
    /// linearized.
    size_t index() const { return _index; }

    template <typename V = Value>
    V* operandAt(size_t index) {
        return const_cast<V*>(
            static_cast<Instruction const*>(this)->operandAt(index));
    }

    template <typename V = Value>
    V const* operandAt(size_t index) const {
        auto* op = ops[index];
        return op ? cast<V const*>(op) : nullptr;
    }

    std::span<Value* const> operands() { return ops; }

    std::span<Value const* const> operands() const { return ops; }

    template <InstructionData T>
    void setInstData(T data) {
        std::memcpy(&_instData, &data, sizeof(T));
    }

    uint64_t instData() const { return _instData; }

    void replaceOperand(Value* old, Value* repl);

    template <InstructionData T>
    T instDataAs() const {
        alignas(T) char buffer[sizeof(T)];
        std::memcpy(&buffer, &_instData, sizeof(T));
        return reinterpret_cast<T&>(buffer);
    }

private:
    friend class Function;
    void setIndex(size_t index) { _index = utl::narrow_cast<uint32_t>(index); }

    InstCode oc; // 2 Bytes
    uint16_t _width;
    uint32_t _index = ~0u;
    Register* _dest;
    utl::small_vector<Value*> ops;
    uint64_t _instData;
};

} // namespace scatha::mir

namespace scatha::mir {

///
///
///
class Constant: public Value {
public:
    Constant(uint64_t value, size_t width):
        Value(NodeType::Constant), val(value), _width(width) {}

    uint64_t value() const { return val; }

    size_t bytewidth() const { return _width; }

    size_t bitwidth() const { return 8 * bytewidth(); }

private:
    uint64_t val  = 0;
    size_t _width = 0;
};

///
///
///
class UndefValue: public Value {
public:
    UndefValue(): Value(NodeType::UndefValue) {}
};

///
///
///
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

    std::string_view name() const { return _name; }

    void addLiveIn(Register* reg, size_t count = 1) {
        addLiveImpl(_liveIn, reg, count);
    }

    void removeLiveIn(Register* reg, size_t count = 1) {
        removeLiveImpl(_liveIn, reg, count);
    }

    void addLiveOut(Register* reg, size_t count = 1) {
        addLiveImpl(_liveOut, reg, count);
    }

    void removeLiveOut(Register* reg, size_t count = 1) {
        removeLiveImpl(_liveOut, reg, count);
    }

    bool isLiveIn(Register const* reg) const { return _liveIn.contains(reg); }

    bool isLiveOut(Register const* reg) const { return _liveOut.contains(reg); }

    auto const& liveIn() const { return _liveIn; }

    auto const& liveOut() const { return _liveOut; }

    bool isEntry() const;

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

///
///
///
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
                      size_t numReturnRegisters);

    /// \Returns The name of this function.
    std::string_view name() const { return _name; }

    /// \Returns The number of registers filled with arguments by the caller.
    size_t numArgumentRegisters() const { return numArgRegs; }

    /// \Returns The number of registers to be filled with the return value by
    /// the callee.
    size_t numReturnValueRegisters() const { return numRetvalRegs; }

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
    size_t numArgRegs    : 20  = 0;
    size_t numRetvalRegs : 20  = 0;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
