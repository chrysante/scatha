#ifndef SCATHA_IR_CFG_FUNCTION_H_
#define SCATHA_IR_CFG_FUNCTION_H_

#include <memory>
#include <span>
#include <string>

#include "Basic/Basic.h"
#include "IR/Attribute.h"
#include "IR/CFG/BasicBlock.h"
#include "IR/CFG/Constant.h"
#include "IR/CFG/UniqueName.h"
#include "IR/Common.h"

namespace scatha::ir {

/// Represents a function parameter.
class SCATHA_API Parameter:
    public Value,
    public ListNode<Parameter>,
    public ParentedNode<Callable> {
    using ParentNodeBase = ParentedNode<Callable>;

public:
    explicit Parameter(Type const* type, size_t index, Callable* parent):
        Parameter(type, index, std::to_string(index), parent) {}

    explicit Parameter(Type const* type,
                       size_t index,
                       std::string name,
                       Callable* parent):
        Value(NodeType::Parameter, type, std::move(name)),
        ParentNodeBase(parent),
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

    /// \Returns The visibility of this callable.
    Visibility visibility() const { return vis; }

protected:
    explicit Callable(NodeType nodeType,
                      FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Type const* const> parameterTypes,
                      std::string name,
                      FunctionAttribute attr,
                      Visibility vis);

    explicit Callable(NodeType nodeType,
                      FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Parameter* const>,
                      std::string name,
                      FunctionAttribute attr,
                      Visibility vis);

private:
    List<Parameter> params;
    Type const* _returnType;
    FunctionAttribute attrs;
    Visibility vis;
};

/// Represents a function. A function is a prototype with a list of basic
/// blocks.
class SCATHA_API Function:
    public Callable,
    public CFGList<Function, BasicBlock>,
    public ListNode<Function>,
    public ParentedNode<Module> {
    friend class CFGList<Function, BasicBlock>;
    using ListBase = CFGList<Function, BasicBlock>;

    template <typename Itr, typename Self>
    static ranges::subrange<Itr> getInstructionsImpl(Self&& self) {
        using InstItr = typename Itr::InstructionIterator;
        Itr const begin(self.begin(),
                        self.empty() ? InstItr{} : self.front().begin());
        Itr const end(self.end(), InstItr{});
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
                      FunctionAttribute attr,
                      Visibility vis = Visibility::Static);

    /// Construct a function with explicit parameters.
    explicit Function(FunctionType const* functionType,
                      Type const* returnType,
                      std::span<Parameter* const> parameters,
                      std::string name,
                      FunctionAttribute attr,
                      Visibility vis = Visibility::Static);

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

    /// Access this functions dominator tree.
    DomTree const& getOrComputeDomTree() const;

    /// Access this functions dominance information.
    DominanceInfo const& getOrComputeDomInfo() const;

    /// Access this functions post-dominance information.
    DominanceInfo const& getOrComputePostDomInfo() const;

    /// Access this functions loop nesting forest.
    LoopNestingForest const& getOrComputeLNF() const;

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

    struct AnalysisData;

    UniqueNameFactory nameFac;
    std::unique_ptr<AnalysisData> analysisData;
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
