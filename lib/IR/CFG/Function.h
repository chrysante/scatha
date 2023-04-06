#ifndef SCATHA_IR_CFG_FUNCTION_H_
#define SCATHA_IR_CFG_FUNCTION_H_

#include <span>
#include <string>

#include "Basic/Basic.h"
#include "IR/Attribute.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/CFGList.h"
#include "IR/CFG/Constant.h"
#include "IR/CFG/UniqueName.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Represents a function parameter.
class SCATHA_API Parameter:
    public Value,
    public NodeWithParent<Parameter, Callable> {
    using NodeBase = NodeWithParent<Parameter, Callable>;

public:
    explicit Parameter(Type const* type, size_t index, Callable* parent):
        Parameter(type, index, std::to_string(index), parent) {}

    explicit Parameter(Type const* type,
                       size_t index,
                       std::string name,
                       Callable* parent):
        Value(NodeType::Parameter, type, std::move(name)),
        NodeBase(parent),
        _index(index) {}

    /// \returns the index of this parameter which may but does not have to be
    /// its name.
    size_t index() const { return _index; }

    void setIndex(size_t index) { _index = index; }

private:
    size_t _index;
};

/// Represents a callable.
/// This is a base common class of `Function` and `ExtFunction`.
class SCATHA_API Callable: public Constant {
    static auto getParametersImpl(auto&& self) {
        /// Trick to return a view over parameters without returning the
        /// `List<>` itself.
        return self.params | ranges::views::transform(
                                 [](auto& param) -> auto& { return param; });
    }

public:
    /// \returns a view over the function parameters
    auto parameters() { return getParametersImpl(*this); }

    /// \overload
    auto parameters() const { return getParametersImpl(*this); }

    /// \returns the return type of this function
    Type const* returnType() const { return _returnType; }

    /// \returns The attribute bitfield of this function.
    FunctionAttribute attributes() const { return attrs; }

    /// \returns `true` iff attribute `attr` is set on this function.
    bool hasAttribute(FunctionAttribute attr) const {
        return test(attrs & attr);
    }

    /// Set attribute `attr` to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute `attr` to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

protected:
    explicit Callable(NodeType nodeType,
                      FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name,
                      FunctionAttribute attr);

    explicit Callable(NodeType nodeType,
                      FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Parameter* const>,
                      std::string name,
                      FunctionAttribute attr);

private:
    List<Parameter> params;
    Type const* _returnType;
    FunctionAttribute attrs;
};

/// Represents a function. A function is a prototype with a list of basic
/// blocks.
class SCATHA_API Function:
    public Callable,
    public internal::CFGList<Function, BasicBlock>,
    public NodeWithParent<Function, Module> {
    friend class internal::CFGList<Function, BasicBlock>;
    using ListBase = internal::CFGList<Function, BasicBlock>;

    template <typename Itr, typename Self>
    static ranges::subrange<Itr> getInstructionsImpl(Self&& self) {
        using InstItr = typename Itr::InstructionIterator;
        Itr const begin(self.values.begin(),
                        self.values.empty() ? InstItr{} :
                                              self.values.front().begin());
        Itr const end(self.values.end(), InstItr{});
        return { begin, end };
    }

public:
    using ListBase::ConstIterator;
    using ListBase::Iterator;

    using InstructionIterator =
        internal::InstructionIteratorImpl<Function::Iterator,
                                          BasicBlock::Iterator>;
    using ConstInstructionIterator =
        internal::InstructionIteratorImpl<Function::ConstIterator,
                                          BasicBlock::ConstIterator>;

    /// Construct a function with parameter types.
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name,
                      FunctionAttribute attr);

    /// Construct a function with explicit parameters.
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Parameter* const> parameters,
                      std::string name,
                      FunctionAttribute attr);

    ~Function();

    /// \returns the entry basic block of this function
    BasicBlock& entry() { return front(); }

    /// \overload
    BasicBlock const& entry() const { return front(); }

    /// \returns a view over all instructions in this `Function`.
    ranges::subrange<InstructionIterator> instructions() {
        return getInstructionsImpl<InstructionIterator>(*this);
    }

    /// \overload
    auto instructions() const {
        return getInstructionsImpl<ConstInstructionIterator>(*this);
    }

    /// Erase all basic blocks and all instructions.
    void clear();

    /// Access this functions dominator tree.
    DomTree const& getOrComputeDomTree();

    /// Access this functions dominance information.
    DominanceInfo const& getOrComputeDomInfo();

    /// Access this functions post-dominance information.
    DominanceInfo const& getOrComputePostDomInfo();

    /// Access this functions loop nesting forest.
    LoopNestingForest const& getOrComputeLNF();

    /// Invalidate (post-) dominance and loop information.
    void invalidateCFGInfo();

private:
    /// Callbacks used by CFGList base class
    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

private:
    /// For access to `nameFac`
    friend class Value;
    friend class BasicBlock;
    /// For access to `invalidateDomInfoEtc()`
    friend class Terminator;

    UniqueNameFactory nameFac;
    std::unique_ptr<DominanceInfo> domInfo;
    std::unique_ptr<DominanceInfo> postDomInfo;
    std::unique_ptr<LoopNestingForest> LNF;
};

/// Represents an external function.
class SCATHA_API ExtFunction: public Callable {
public:
    explicit ExtFunction(FunctionType const* functionType,
                         Type const* returnType,
                         std::span<Type const* const> parameterTypes,
                         std::string name,
                         uint32_t slot,
                         uint32_t index,
                         FunctionAttribute attr);

    /// Slot in external function table of VM.
    size_t slot() const { return _slot; }

    /// Index into slot.
    size_t index() const { return _index; }

private:
    uint32_t _slot, _index;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_FUNCTION_H_
