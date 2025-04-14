#ifndef SCATHA_ASM_BLOCK_H_
#define SCATHA_ASM_BLOCK_H_

#include <string>
#include <string_view>

#include <range/v3/range.hpp>
#include <utl/vector.hpp>

#include "Assembly/Instruction.h"
#include "Assembly/Value.h"

namespace scatha::Asm {

struct BlockOptions {
    bool isExternallyVisible = false;
    bool isFunction = false;
};

class Block {
public:
    using Iterator = utl::vector<Instruction>::iterator;
    using ConstIterator = utl::vector<Instruction>::const_iterator;

    explicit Block(LabelID id, std::string name, BlockOptions options = {}):
        Block(id, std::move(name), options, {}) {}

    explicit Block(LabelID id, std::string name,
                   std::initializer_list<Instruction> instructions):
        Block(id, std::move(name), {}, instructions) {}

    explicit Block(LabelID id, std::string name, BlockOptions options,
                   std::initializer_list<Instruction> instructions):
        _id(id),
        _extern(options.isExternallyVisible),
        _isFunction(options.isFunction),
        _name(std::move(name)),
        instructions(instructions) {}

    LabelID id() const { return _id; }

    /// \Returns `true` if the address of this block should appear in the
    /// global symbol table.
    bool isExternallyVisible() const { return _extern; }

    /// \Returns true if this block is the entry block of a function
    bool isFunction() const { return _isFunction; }

    std::string const& name() const { return _name; }

    size_t instructionCount() const { return instructions.size(); }
    bool empty() const { return instructions.empty(); }

    auto begin() { return instructions.begin(); }
    auto begin() const { return instructions.begin(); }

    auto end() { return instructions.end(); }
    auto end() const { return instructions.end(); }

    auto& back() { return instructions.back(); }
    auto& back() const { return instructions.back(); }

    void insertBack(Instruction const& instruction) {
        instructions.push_back(instruction);
    }
    void insert(ConstIterator position, Instruction const& instruction) {
        instructions.insert(position, instruction);
    }
    void insert(ConstIterator position, ranges::range auto&& instructions) {
        this->instructions.insert(position, ranges::begin(instructions),
                                  ranges::end(instructions));
    }

private:
    LabelID _id      : 62;
    bool _extern     : 1 = false;
    bool _isFunction : 1 = false;
    std::string _name;
    utl::vector<Instruction> instructions;
};

} // namespace scatha::Asm

#endif // SCATHA_ASM_BLOCK_H_
