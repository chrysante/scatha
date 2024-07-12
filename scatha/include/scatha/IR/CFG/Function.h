#ifndef SCATHA_IR_CFG_FUNCTION_H_
#define SCATHA_IR_CFG_FUNCTION_H_

#include <memory>
#include <span>
#include <string>

#include <scatha/Common/Base.h>
#include <scatha/Common/FFI.h>
#include <scatha/IR/CFG/BasicBlock.h>
#include <scatha/IR/CFG/Global.h>
#include <scatha/IR/Fwd.h>
#include <scatha/IR/UniqueName.h>

namespace scatha::ir {

/// Represents a function parameter.
class SCATHA_API Parameter:
    public Value,
    public ListNode<Parameter>,
    public ParentedNode<Callable> {
    using ParentNodeBase = ParentedNode<Callable>;

public:
    explicit Parameter(Type const* type, size_t index, Callable* parent);

    explicit Parameter(Type const* type, size_t index, std::string name,
                       Callable* parent);

    /// \returns the index of this parameter which may but does not have to be
    /// its name.
    size_t index() const { return _index; }

    void setIndex(size_t index) { _index = index; }

private:
    size_t _index;
};

/// Allocates `ir::Parameter` nodes according to \p types with names `"0", "1",
/// ...`
List<Parameter> makeParameters(std::span<Type const* const> types);

/// Represents a callable.
/// This is a base common class of `Function` and `ForeignFunction`.
class SCATHA_API Callable: public Global {
public:
    /// \returns The function parameters
    List<Parameter>& parameters() { return params; }

    /// \overload
    List<Parameter> const& parameters() const { return params; }

    /// \returns the return type of this function
    Type const* returnType() const { return _returnType; }

    /// \returns The attribute bitfield of this function.
    FunctionAttribute attributes() const { return attrs; }

    /// \returns `true` if attribute `attr` is set on this function.
    bool hasAttribute(FunctionAttribute attr) const {
        return test(attrs & attr);
    }

    /// Set attribute `attr` to `true`.
    void setAttribute(FunctionAttribute attr) { attrs |= attr; }

    /// Set attribute `attr` to `false`.
    void removeAttribute(FunctionAttribute attr) { attrs &= ~attr; }

protected:
    explicit Callable(NodeType nodeType, Context& ctx, Type const* returnType,
                      List<Parameter> parameters, std::string name,
                      FunctionAttribute attr, Visibility vis);

private:
    List<Parameter> params;
    Type const* _returnType;
    FunctionAttribute attrs;
};

/// Represents a function. A function is a prototype with a list of basic
/// blocks.
class SCATHA_API Function:
    public Callable,
    public CFGList<Function, BasicBlock> {
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

    explicit Function(Context& ctx, Type const* returnType,
                      List<Parameter> parameters, std::string name,
                      FunctionAttribute attr,
                      Visibility vis = Visibility::Internal);

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
    DomTree& getOrComputeDomTree() {
        return const_cast<DomTree&>(
            static_cast<Function const*>(this)->getOrComputeDomTree());
    }

    /// \overload
    DomTree const& getOrComputeDomTree() const;

    /// Access this functions dominance information.
    DominanceInfo& getOrComputeDomInfo() {
        return const_cast<DominanceInfo&>(
            static_cast<Function const*>(this)->getOrComputeDomInfo());
    }

    /// \overload
    DominanceInfo const& getOrComputeDomInfo() const;

    /// Access this functions post-dominance information.
    DominanceInfo& getOrComputePostDomInfo() {
        return const_cast<DominanceInfo&>(
            static_cast<Function const*>(this)->getOrComputePostDomInfo());
    }

    /// \overload
    DominanceInfo const& getOrComputePostDomInfo() const;

    /// Access this functions loop nesting forest.
    LoopNestingForest& getOrComputeLNF() {
        return const_cast<LoopNestingForest&>(
            static_cast<Function const*>(this)->getOrComputeLNF());
    }

    /// \overload
    LoopNestingForest const& getOrComputeLNF() const;

    /// Invalidate (post-) dominance and loop information.
    void invalidateCFGInfo();

private:
    /// Callbacks used by CFGList base class
    void insertCallback(BasicBlock&);
    void eraseCallback(BasicBlock const&);

    /// For access to `nameFac`
    friend class Value;
    friend class BasicBlock;

    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;

    struct AnalysisData;

    UniqueNameFactory nameFac;
    std::unique_ptr<AnalysisData> analysisData;
};

/// Represents a foreign function.
class SCATHA_API ForeignFunction: public Callable {
public:
    explicit ForeignFunction(Context& ctx, Type const* returnType,
                             List<Parameter> parameters, std::string name,
                             FunctionAttribute attr);

    /// Returns the common FFI representation of this function
    ForeignFunctionInterface const& getFFI() const { return ffi; }

private:
    friend class Constant;
    void writeValueToImpl(
        void* dest,
        utl::function_view<void(Constant const*, void*)> callback) const;

    ForeignFunctionInterface ffi;
};

} // namespace scatha::ir

#endif // SCATHA_IR_CFG_FUNCTION_H_
