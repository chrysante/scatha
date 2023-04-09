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
class Instruction: public ListNodeWithParent<Instruction, BasicBlock> {
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
                         T instData):
        Instruction(opcode, dest, std::move(operands), convInstData(instData)) {
    }

    explicit Instruction(InstCode opcode,
                         Register* dest,
                         utl::small_vector<Value*> operands = {},
                         uint64_t instData                  = 0);

    void setDest(Register* dest);

    void setOperands(utl::small_vector<Value*> operands);

    void clearOperands();

    InstCode instcode() const { return oc; }

    Register* dest() { return _dest; }

    Register const* dest() const { return _dest; }

    template <typename V = Value>
    V* operandAt(size_t index) {
        return cast<V*>(ops[index]);
    }

    template <typename V = Value>
    V const* operandAt(size_t index) const {
        return cast<V const*>(ops[index]);
    }

    std::span<Value* const> operands() { return ops; }

    std::span<Value const* const> operands() const { return ops; }

    uint64_t instData() const { return _instData; }

    template <InstructionData T>
    T instDataAs() const {
        alignas(T) char buffer[sizeof(T)];
        std::memcpy(&buffer, &_instData, sizeof(T));
        return reinterpret_cast<T&>(buffer);
    }

private:
    InstCode oc;
    Register* _dest;
    utl::small_vector<Value*> ops;
    uint64_t _instData;
};

} // namespace scatha::mir

namespace scatha::mir {

///
///
///
class Value {
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
class Register: public Value, public ListNodeWithParent<Register, Function> {
public:
    static constexpr size_t InvalidIndex = ~size_t(0);

    explicit Register(size_t index): Value(NodeType::Register), _index(index) {}

    size_t index() const { return _index; }

    void setIndex(size_t index) { _index = index; }

private:
    friend class Instruction;
    void addDef(Instruction* inst);
    void removeDef(Instruction* inst);
    void addUser(Instruction* inst);
    void removeUser(Instruction* inst);

    size_t _index;
    utl::hashset<Instruction*> _defs;
    utl::hashmap<Instruction*, size_t> _users;
};

///
///
///
class Constant: public Value {
public:
    Constant(uint64_t value): Value(NodeType::Constant), val(value) {}

    uint64_t value() const { return val; }

private:
    uint64_t val;
};

///
///
///
class BasicBlock:
    public Value,
    public CFGList<BasicBlock, Instruction>,
    public ListNodeWithParent<BasicBlock, Function>,
    public GraphNode<void, BasicBlock, GraphKind::Directed> {
    using ListBase = CFGList<BasicBlock, Instruction>;

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    explicit BasicBlock(std::string name):
        Value(NodeType::BasicBlock), _name(std::move(name)) {}

    std::string_view name() const { return _name; }

    void addLiveIn(Register* reg) { addLiveImpl(_liveIn, reg); }

    void removeLiveIn(Register* reg) { removeLiveImpl(_liveIn, reg); }

    void addLiveOut(Register* reg) { addLiveImpl(_liveOut, reg); }

    void removeLiveOut(Register* reg) { removeLiveImpl(_liveOut, reg); }

    auto const& liveIn() const { return _liveIn; }

    auto const& liveOut() const { return _liveOut; }

    bool isEntry() const;

private:
    friend class Function;
    friend class CFGList<BasicBlock, Instruction>;

    void insertCallback(Instruction& inst);
    void eraseCallback(Instruction const& inst);

    static void addLiveImpl(utl::hashset<Register*>& set, Register* reg) {
        set.insert(reg);
    }
    static void removeLiveImpl(utl::hashset<Register*>& set, Register* reg) {
        auto itr = set.find(reg);
        SC_ASSERT(itr != set.end(), "Not found");
        set.erase(itr);
    }

    std::string _name;
    utl::hashset<Register*> _liveIn, _liveOut;
};

///
///
///
class Function:
    public Value,
    public CFGList<Function, BasicBlock>,
    public ListNode<Function> {
    using ListBase = CFGList<Function, BasicBlock>;

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    explicit Function(std::string name):
        Value(NodeType::Function), _name(std::move(name)) {}

    std::string_view name() const { return _name; }

    auto registers() {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto registers() const {
        return regs | ranges::views::transform([](auto& p) { return &p; });
    }

    auto regBegin() { return regs.begin(); }

    auto regBegin() const { return regs.begin(); }

    auto regEnd() { return regs.end(); }

    auto regEnd() const { return regs.end(); }

    bool regEmpty() const { return regs.empty(); }

    void addRegister(Register* reg) { regs.push_back(reg); }

    BasicBlock* entry() { return &front(); }

    BasicBlock const* entry() const { return &front(); }

private:
    friend class CFGList<Function, BasicBlock>;

    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    std::string _name;
    List<Register> regs;
};

} // namespace scatha::mir

#endif // SCATHA_MIR_CFG_H_
