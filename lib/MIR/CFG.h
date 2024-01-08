#ifndef SCATHA_MIR_CFG_H_
#define SCATHA_MIR_CFG_H_

#include <array>
#include <span>
#include <variant>

#include <range/v3/view.hpp>
#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include "Common/FFI.h"
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
    public CFGList<BasicBlock, Instruction>,
    public ProgramPoint {
public:
    using CFGList::ConstIterator;
    using CFGList::Iterator;

    /// Construct an MIR basic block from an IR basic block and a name
    explicit BasicBlock(std::string name, ir::BasicBlock const* irBB = nullptr);

    /// \overload for `BasicBlock(irBB->name(), irBB)`
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

    template <typename PI>
    static auto phiNodesImpl(auto* self) {
        auto itr = self->begin();
        while (itr != self->end() && isa<PI>(*itr)) {
            ++itr;
        }
        return ranges::subrange<decltype(itr)>(self->begin(), itr);
    }

    /// \Returns a view over the phi instructions in this block. Contains only
    /// well formed phi instructions, i.e. the ones at the start of the block
    template <typename PI = PhiInst>
    auto phiNodes() {
        return phiNodesImpl<PI>(this);
    }

    /// \overload
    template <typename PI = PhiInst>
    auto phiNodes() const {
        return phiNodesImpl<PI const>(this);
    }

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

/// Abstract basae class of `Function` and `ForeignFunction`
class Callable: public ListNodeOverride<Callable, Value> {
public:
    /// \Returns The name of this function.
    std::string_view name() const { return _name; }

protected:
    Callable(NodeType type, std::string name);

private:
    std::string _name;
};

/// Represents a function
class Function:
    public ListNodeOverride<Function, Callable>,
    public CFGList<Function, BasicBlock> {
public:
    using CFGList::ConstIterator;
    using CFGList::Iterator;

    /// Construct a `mir::Function` referencing \p irFunc with \p numRegisters
    /// number of virtual registers.
    explicit Function(ir::Function const* irFunc,
                      size_t numArgRegisters,
                      size_t numReturnRegisters,
                      Visibility vis);

    /// \Returns the register phase this function is in
    RegisterPhase registerPhase() const { return regPhase; }

    /// Sets the register to \p phase
    /// \Note this clears all previous phase registers
    void setRegisterPhase(RegisterPhase phase);

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

    /// Concatenated view of virtual registers and callee registers
    auto virtAndCalleeRegs() {
        using namespace ranges::views;
        return concat(virtualRegisters() | transform(cast<mir::Register&>),
                      calleeRegisters());
    }

    /// \overload
    auto virtAndCalleeRegs() const {
        using namespace ranges::views;
        return concat(virtualRegisters() |
                          transform(cast<mir::Register const&>),
                      calleeRegisters());
    }

    /// # Hardware registers

    /// \Returns The set of hardware registers used by this function.
    RegisterSet<HardwareRegister>& hardwareRegisters() { return hardwareRegs; }

    ///  \overload
    RegisterSet<HardwareRegister> const& hardwareRegisters() const {
        return hardwareRegs;
    }

    /// Concatenated view of all registers
    auto allRegisters() {
        using namespace ranges::views;
        return concat(ssaRegisters() | transform(cast<mir::Register&>),
                      virtualRegisters(),
                      calleeRegisters(),
                      hardwareRegisters());
    }

    /// \overload
    auto allRegisters() const {
        using namespace ranges::views;
        return concat(ssaRegisters() | transform(cast<mir::Register const&>),
                      virtualRegisters(),
                      calleeRegisters(),
                      hardwareRegisters());
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
    void linearize();

    /// \pre `linearize()` must have been called
    std::span<std::variant<Instruction*, BasicBlock*> const> programPoints() {
        return progPoints;
    }

    /// \pre `linearize()` must have been called
    auto linearInstructions() {
        return programPoints() | ranges::views::filter([](auto v) {
            return std::holds_alternative<Instruction*>(v);
        }) | ranges::views::transform([](auto v) {
            return std::get<Instruction*>(v);
        });
    }

    /// \Returns The visibility of this function, i.e. `extern` or `static`
    Visibility visibility() const { return vis; }

private:
    friend class CFGList<Function, BasicBlock>;

    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    RegisterSet<SSARegister> ssaRegs;
    RegisterSet<VirtualRegister> virtRegs;
    RegisterSet<CalleeRegister> calleeRegs;
    RegisterSet<HardwareRegister> hardwareRegs;

    /// Flat array of pointers to instructions in this function. Populated by
    /// `linearize()`.
    utl::vector<std::variant<Instruction*, BasicBlock*>> progPoints;

    ir::Function const* irFunc = nullptr;
    size_t numArgRegs    : 20 = 0;
    size_t numRetvalRegs : 20 = 0;
    Visibility vis;
    RegisterPhase regPhase = RegisterPhase::SSA;
};

///
class ForeignFunction: public ListNodeOverride<ForeignFunction, Callable> {
public:
    explicit ForeignFunction(ForeignFunctionInterface FFI):
        ListNodeOverride<ForeignFunction, Callable>(NodeType::ForeignFunction,
                                                    FFI.name()),
        ffi(std::move(FFI)) {}

    ///
    ForeignFunctionInterface const& getFFI() const { return ffi; }

private:
    ForeignFunctionInterface ffi;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
