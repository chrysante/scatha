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

    void clearOperands();

    InstCode instcode() const { return oc; }

    size_t bytewidth() const { return _width; }

    size_t bitwidth() const { return 8 * bytewidth(); }

    Register* dest() { return _dest; }

    Register const* dest() const { return _dest; }

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

    uint64_t instData() const { return _instData; }

    void replaceOperand(Value* old, Value* repl);

    template <InstructionData T>
    T instDataAs() const {
        alignas(T) char buffer[sizeof(T)];
        std::memcpy(&buffer, &_instData, sizeof(T));
        return reinterpret_cast<T&>(buffer);
    }

private:
    InstCode oc;
    uint32_t _width;
    Register* _dest;
    utl::small_vector<Value*> ops;
    uint64_t _instData;
};

} // namespace scatha::mir

namespace scatha::mir {

///
///
///
class Value: public ListNode<Value, /* AllowSetSiblings = */ true> {
public:
    NodeType nodeType() const { return _nodeType; }

protected:
    Value(NodeType nodeType): _nodeType(nodeType) {}

private:
    NodeType _nodeType;
};

inline NodeType dyncast_get_type(Value const& value) {
    return value.nodeType();
}

///
///
///
class Register:
    public ListNodeOverride<Register, Value>,
    public ParentedNode<Function> {
public:
    static constexpr size_t InvalidIndex = ~size_t(0);

    Register(): ListNodeOverride<Register, Value>(NodeType::Register) {}

    size_t index() const { return _index; }

    bool isVirtual() const { return isVirt; }

    void setVirtual(bool value = true) { isVirt = value; }

    auto defs() {
        return _defs | ranges::views::transform([](auto* d) { return d; });
    }

    auto defs() const {
        return _defs | ranges::views::transform([](auto* d) { return d; });
    }

    auto uses() {
        return _users |
               ranges::views::transform([](auto& p) { return p.first; });
    }

    auto uses() const {
        return _users |
               ranges::views::transform([](auto& p) { return p.first; });
    }

private:
    friend class Instruction;

    void addDef(Instruction* inst);
    void removeDef(Instruction* inst);
    void addUser(Instruction* inst);
    void removeUser(Instruction* inst);

    friend class Function;
    /// Index is only set by `Function`
    void setIndex(size_t index) { _index = index; }

    size_t _index : 63 = ~size_t{ 0 };
    bool isVirt   : 1  = false;
    utl::hashset<Instruction*> _defs;
    utl::hashmap<Instruction*, size_t> _users;
};

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
    /// number of registers.
    explicit Function(ir::Function const* irFunc,
                      size_t numArgRegisters,
                      size_t numReturnRegisters);

    /// \Returns The name of this function.
    std::string_view name() const { return _name; }

    /// \Returns View over the registers used by this function.
    auto registers() {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    /// \overload
    auto registers() const {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto virtualRegisters() {
        return virtRegs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto virtualRegisters() const {
        return virtRegs | ranges::views::transform([](auto& p) { return &p; });
    }

    /// \Returns The register with index \p index
    Register* registerAt(size_t index) { return flatRegs[index]; }

    /// \overload
    Register const* registerAt(size_t index) const { return flatRegs[index]; }

    auto regBegin() { return regs.begin(); }

    auto regBegin() const { return regs.begin(); }

    auto regEnd() { return regs.end(); }

    auto regEnd() const { return regs.end(); }

    bool regEmpty() const { return regs.empty(); }

    /// Add a new register to this function.
    /// \Returns The newly added register.
    Register* addRegister();

    void eraseRegister(Register* reg);

    /// Rename all registers of this function from 0 to N
    /// After removal of registers, e.g. due to coalescing, the register indices
    /// may not be consecutive anymore. This function will clean that up.
    void renameRegisters();

    auto virtRegBegin() { return virtRegs.begin(); }

    auto virtRegBegin() const { return virtRegs.begin(); }

    auto virtRegEnd() { return virtRegs.end(); }

    auto virtRegEnd() const { return virtRegs.end(); }

    bool virtRegEmpty() const { return virtRegs.empty(); }

    void addVirtualRegister(Register* reg);

    void clearVirtualRegisters() { virtRegs.clear(); }

    /// \Returns Pointer to the entry basic block
    BasicBlock* entry() { return &front(); }

    /// \overload
    BasicBlock const* entry() const { return &front(); }

    /// \Return Pointer to the `ir::Function` corresponding to this
    /// `mir::Function`.
    ir::Function const* irFunction() const { return irFunc; }

    /// \Returns The number of registers local to this function, excluding
    /// arguments passed to callees and callee metadata.
    size_t numLocalRegisters() const { return numLocalRegs; }

    /// Set the number of registers local to this function. This should be
    /// called be `devirtualizeCalls()`.
    void setNumLocalRegisters(size_t count) { numLocalRegs = count; }

    /// \Returns Number of registers used by this function. After devirtualizing
    /// calls this includes the arguments copied in the higher callee registers
    /// and the registers for call metadata.
    size_t numUsedRegisters() const { return flatRegs.size(); }

    /// Registers used by the arguments to this function.
    std::span<Register* const> argumentRegisters() {
        return std::span<Register* const>(flatRegs.data(), numArgRegs);
    }

    /// \overload
    std::span<Register const* const> argumentRegisters() const {
        return std::span<Register const* const>(flatRegs.data(), numArgRegs);
    }

    /// Registers used for the return value of this function.
    std::span<Register* const> returnValueRegisters() {
        return std::span<Register* const>(flatRegs.data(), numRetvalRegs);
    }

    /// \overload
    std::span<Register const* const> returnValueRegisters() const {
        return std::span<Register const* const>(flatRegs.data(), numRetvalRegs);
    }

private:
    friend class CFGList<Function, BasicBlock>;

    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    std::string _name;
    List<Register> regs;
    utl::vector<Register*> flatRegs;
    List<Register> virtRegs;
    ir::Function const* irFunc = nullptr;
    size_t numLocalRegs  : 20  = 0;
    size_t numArgRegs    : 20  = 0;
    size_t numRetvalRegs : 20  = 0;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
