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
        return cast<V*>(ops[index]);
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

    explicit Register(size_t index):
        ListNodeOverride<Register, Value>(NodeType::Register), _index(index) {}

    size_t index() const { return _index; }

    void setIndex(size_t index) { _index = index; }

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

    size_t _index : 63;
    bool isVirt   : 1 = false;
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

    explicit Function(ir::Function const* irFunc);

    std::string_view name() const { return _name; }

    auto registers() {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto registers() const {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto virtualRegisters() {
        return virtRegs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto virtualRegisters() const {
        return virtRegs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto regBegin() { return regs.begin(); }

    auto regBegin() const { return regs.begin(); }

    auto regEnd() { return regs.end(); }

    auto regEnd() const { return regs.end(); }

    bool regEmpty() const { return regs.empty(); }

    void addRegister(Register* reg);

    auto virtRegBegin() { return virtRegs.begin(); }

    auto virtRegBegin() const { return virtRegs.begin(); }

    auto virtRegEnd() { return virtRegs.end(); }

    auto virtRegEnd() const { return virtRegs.end(); }

    bool virtRegEmpty() const { return virtRegs.empty(); }

    void addVirtualRegister(Register* reg);

    void clearVirtualRegisters() { virtRegs.clear(); }

    BasicBlock* entry() { return &front(); }

    BasicBlock const* entry() const { return &front(); }

    ir::Function const* irFunction() const { return irFunc; }

    size_t numLocalRegisters() const { return localRegs; }

    void setNumLocalRegisters(size_t count) { localRegs = count; }

    std::span<Register* const> argumentRegisters() { return argRegs; }

    std::span<Register const* const> argumentRegister() const {
        return argRegs;
    }

    void setArgumentRegisters(utl::small_vector<Register*> regs) {
        argRegs = std::move(regs);
    }

    std::span<Register* const> returnRegisters() { return retRegs; }

    std::span<Register const* const> returnRegisters() const { return retRegs; }

    void setReturnRegisters(utl::small_vector<Register*> regs) {
        retRegs = std::move(regs);
    }

private:
    friend class CFGList<Function, BasicBlock>;

    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    std::string _name;
    List<Register> regs;
    List<Register> virtRegs;
    ir::Function const* irFunc = nullptr;
    size_t localRegs           = 0;
    utl::small_vector<Register*> argRegs, retRegs;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
