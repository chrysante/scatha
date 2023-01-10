#ifndef SCATHA_OPT_CONTROLFLOWPATH_H_
#define SCATHA_OPT_CONTROLFLOWPATH_H_

#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "IR/CFG.h"

namespace scatha::opt {

class SCATHA(TEST_API) ControlFlowPath {
    template <typename Super>
    class IteratorBase {
        friend class ControlFlowPath;
        
        using BBItr = utl::vector<ir::BasicBlock const*>::const_iterator;
        using InstItr = ir::List<ir::Instruction>::const_iterator;
        
        IteratorBase(ControlFlowPath const* path, BBItr bbItr, InstItr instItr):
            path(path), bbItr(bbItr), instItr(instItr) {
            static_cast<Super*>(this)->onConstruction();
        }
        
    public:
        using size_type         = size_t;
        using difference_type   = ssize_t;
        using value_type        = ir::Instruction const;
        using pointer           = value_type*;
        using reference         = value_type&;
        using iterator_category = std::input_iterator_tag;
        
        bool operator==(IteratorBase const& rhs) const {
            SC_ASSERT(path == rhs.path, "Iterators are not comparable.");
            return bbItr == rhs.bbItr && instItr == rhs.instItr;
        }

        ir::Instruction const& instruction() const { return *instItr; }
        
        ir::BasicBlock const& basicBlock() const { return *static_cast<Super const*>(this)->currentBB(); }
        
        ir::Instruction const& operator*() const { return instruction(); }
        
        ir::Instruction const* operator->() const { return instItr.to_address(); }
        
    protected:
        ControlFlowPath const* path;
        BBItr bbItr;
        InstItr instItr;
    };
    
public:
    class Iterator: public IteratorBase<Iterator> {
        friend class ControlFlowPath;

        using IteratorBase::IteratorBase;
        
        ir::BasicBlock const* currentBB() const { return *bbItr; }
        
        void handleBBBoundary() {
            if (instItr == currentBB()->instructions.end()) {
                ++bbItr;
                if (bbItr != path->_bbs.end()) {
                    SC_ASSERT(!currentBB()->instructions.empty(),
                              "If a BB has no instructions, this traversal does not work so easily. "
                              "But then this BB is not well formed anyway.");
                    instItr = currentBB()->instructions.begin();
                }
            }
        }
        
        void onConstruction() {
            handleBBBoundary();
        }
        
    public:
        Iterator& operator++() {
            SC_ASSERT(bbItr != path->_bbs.end(), "Iterator not incrementable");
            ++instItr;
            handleBBBoundary();
            return *this;
        }
        
        Iterator operator++(int) {
            auto result = *this;
            ++*this;
            return result;
        }
        
        bool operator==(Iterator const& rhs) const = default;
    };
    
    class ReverseIterator: public IteratorBase<ReverseIterator> {
        friend class ControlFlowPath;

        using IteratorBase::IteratorBase;
        
        ir::BasicBlock const* currentBB() const { return *std::prev(bbItr); }
        
        void handleBBBoundary() {
            if (bbItr == path->_bbs.begin()) {
                return;
            }
            if (instItr != currentBB()->instructions.end()) {
                return;
            }
            --bbItr;
            if (bbItr == path->_bbs.begin()) {
                return;
            }
            SC_ASSERT(!currentBB()->instructions.empty(),
                      "If a BB has no instructions, this traversal does not work so easily. "
                      "But then this BB is not well formed anyway.");
            instItr = std::prev(currentBB()->instructions.end());
        }
        
        void onConstruction() {
            handleBBBoundary();
        }
        
    public:
        ReverseIterator& operator++() {
            SC_ASSERT(bbItr != path->_bbs.begin(), "Iterator not incrementable");
            --instItr;
            handleBBBoundary();
            return *this;
        }
        
        ReverseIterator operator++(int) {
            auto result = *this;
            ++*this;
            return result;
        }
        
        bool operator==(ReverseIterator const& rhs) const = default;
    };
    
    explicit ControlFlowPath(ir::Instruction const* from, utl::small_vector<ir::BasicBlock const*> bbs, ir::Instruction const* to):
        _bbs(std::move(bbs)),
        _beginInst(from),
        _backInst(to) {}
    
    explicit ControlFlowPath(ir::Instruction const* from, std::span<ir::BasicBlock const* const> bbs, ir::Instruction const* to):
        _bbs(bbs),
        _beginInst(from),
        _backInst(to) {}
    
    bool valid() const;
    
    Iterator begin() const { return Iterator(this, _bbs.begin(), Iterator::InstItr(_beginInst)); }
    
    Iterator end() const { return Iterator(this, _bbs.end() - 1, Iterator::InstItr(_backInst->next())); }
    
    ReverseIterator rbegin() const { return ReverseIterator(this, _bbs.end(), Iterator::InstItr(_backInst)); }
    
    ReverseIterator rend() const { return ReverseIterator(this, _bbs.begin() + 1, Iterator::InstItr(_beginInst->prev())); }
    
private:
    utl::small_vector<ir::BasicBlock const*> _bbs;
    ir::Instruction const* _beginInst;
    ir::Instruction const* _backInst; // Note: This is _not_ past the end.
};

} // namespace scatha::opt

#endif // SCATHA_OPT_CONTROLFLOWPATH_H_

