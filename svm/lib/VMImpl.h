#ifndef SVM_VMIMPL_H_
#define SVM_VMIMPL_H_

#include <span>

#include <utl/dynamic_library.hpp>
#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "Common.h"
#include "ExternalFunction.h"
#include "VMData.h"
#include "VirtualMemory.h"

namespace svm {

class VirtualMachine;

/// Implementation details of the virtual machine
struct VMImpl {
    VMImpl();

    VirtualMachine* parent = nullptr;

    std::vector<BuiltinFunction> builtinFunctionTable;

    std::vector<ForeignFunction> foreignFunctionTable;

    CompareFlags cmpFlags{};

    /// Stack size of this VM. Will be set on construction
    size_t stackSize = 0;

    /// Memory for registers
    utl::vector<u64> registers;

    /// Begin of the binary section
    u8 const* binary;

    /// Size of the binary section
    size_t binarySize = 0;

    /// End of binary section
    u8 const* programBreak = nullptr;

    /// Optional address of the `main`/`start` function.
    size_t startAddress = 0;

    /// The VM has a stack of execution contexts instead of a single one to
    /// allow nested invocations of the same program in the same VM instance via
    /// host callbacks.
    utl::stack<ExecutionFrame, 4> execFrames;

    /// The currently active execution frame
    ExecutionFrame currentFrame;

    /// Statistics
    VMStats stats;

    /// Memory of this VM. All memory that the program uses is allocated through
    /// this as well as static memory and stack memory.
    VirtualMemory memory;

    std::istream* istream;

    std::ostream* ostream;

    std::vector<utl::dynamic_library> dylibs;

    std::filesystem::path libdir;

    /// See documentation in "VirtualMachine.h"
    /// @{
    u64 const* execute(size_t startAddress, std::span<u64 const> arguments);
    void beginExecution(size_t startAddress, std::span<u64 const> arguments);
    bool running() const;
    void stepExecution();
    u64 const* endExecution();
    size_t instructionPointerOffset() const;
    void setInstructionPointerOffset(size_t offset);
    /// @}
};

} // namespace svm

#endif // SVM_VMIMPL_H_
