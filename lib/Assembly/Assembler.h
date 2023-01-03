#ifndef SCATHA_CODEGEN_ASSEMBLER_H_
#define SCATHA_CODEGEN_ASSEMBLER_H_

#include <array>
#include <iosfwd>

#include <utl/hashmap.hpp>
#include <utl/utility.hpp>
#include <utl/vector.hpp>

#include "Assembly/Assembly.h"
#include "Assembly/AssemblyStream.h"
#include "Basic/Basic.h"
#include "VM/OpCode.h"
#include "VM/Program.h"

namespace scatha::assembly {

/// ** TODO **
/// We need to rework the entire assembly module. We need to have better iterators into the assembly stream and we
/// need to insert instructions at random positions in order to implement PHI nodes.

/*
 Before being assembled the program looks like this:
 ---------------------------------------------------
 [8 bit marker] -- some data --
 ...
 ...
 ...
 ---------------------------------------------------

 Markers are defined in "Assembly.h".
 They are stripped from the program in assembly stage.
 After assembly the executable looks like this:
 ---------------------------------------------------
 [8 bit opcode][arguments and data]
 ...
 ---------------------------------------------------
 */

struct AssemblerOptions {
    u64 mainID = u64(-1);
};

class SCATHA(API) Assembler {
public:
    explicit Assembler(AssemblyStream const&);
    vm::Program assemble(AssemblerOptions = {});

private:
    struct LabelPlaceholder {};

    void processInstruction(Instruction, StreamIterator&);
    void processUnaryInstruction(Instruction, StreamIterator&);
    void processBinaryInstruction(Instruction, StreamIterator&);
    void processJump(Instruction, StreamIterator&);

    void registerLabel(Label);
    void registerJumpsite(StreamIterator&);

    void postProcess();

    size_t currentPosition() const { return program->instructions.size(); }

    /** MARK: put
     * Family of functions for inserting data into the program during assembly
     */
    void put(vm::OpCode);
    void put(LabelPlaceholder);
    void put(Element const&);
    // delete these overloads to prevent infinite recursion when calling
    // put(Element const&), because when the Element variant holds the
    // Instruction or Label alternative, we would implicitly construct an
    // Element from it and call put(Element const&) again.
    void put(Instruction) = delete;
    void put(Label)       = delete;
    void put(RegisterIndex);
    void put(MemoryAddress);
    void put(Value8);
    void put(Value16);
    void put(Value32);
    void put(Value64);

private:
    struct Jumpsite {
        size_t index;
        size_t line;
        Label label;
    };

    // The AssemblyStream we are processing
    AssemblyStream const& stream;
    // Pointer to the program we are assembling, so we don't have to pass
    // it around the call tree
    vm::Program* program = nullptr;
    // Mapping Label ID -> Code position
    utl::hashmap<Label, size_t> labels;
    // List of all code position with a jump site
    utl::vector<Jumpsite> jumpsites;
};

} // namespace scatha::assembly

#endif // SCATHA_CODEGEN_ASSEMBLER_H_
