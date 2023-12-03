#ifndef SCATHA_MIR_CFG_H_
#define SCATHA_MIR_CFG_H_

#include <array>
#include <span>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/Graph.h"
#include "Common/List.h"
#include "Common/Metadata.h"
#include "Common/UniquePtr.h"
#include "MIR/Fwd.h"
#include "MIR/Instruction.h"
#include "MIR/RegisterSet.h"
#include "MIR/Value.h"

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

    /// \Returns `true` if register \p reg is live-in to this block
    bool isLiveIn(Register const* reg) const { return _liveIn.contains(reg); }

    /// \Returns `true` if register \p reg is live-out of this block
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
