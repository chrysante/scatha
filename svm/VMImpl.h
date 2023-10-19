#ifndef SVM_VMIMPL_H_
#define SVM_VMIMPL_H_

#include <span>

#include <utl/stack.hpp>
#include <utl/vector.hpp>

#include "svm/Common.h"
#include "svm/VirtualMemory.h"

namespace svm {

class VirtualMachine;

struct VMFlags {
    bool less  : 1;
    bool equal : 1;
};

/// Represents the state of an invocation of the virtual machine.
struct ExecutionFrame {
    u64* regPtr = nullptr;
    u64* bottomReg = nullptr;
    u8 const* iptr = nullptr;
    VirtualPointer stackPtr{};
};

struct VMStats {
    size_t executedInstructions = 0;
};

/// Implementation details of the virtual machine
struct VMImpl {
    VirtualMachine* parent = nullptr;

    std::vector<std::vector<ExternalFunction>> extFunctionTable;

    VMFlags flags{};

    /// Stack pointer. Will be set when a binary is loaded.
    u8* stackPtr = nullptr;

    /// Stack size of this VM. Will be set on construction
    size_t stackSize = 0;

    /// Memory for registers
    utl::vector<u64> registers;

    /// Begin of the binary section
    u8 const* binary;

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

    ///
    u64 const* execute(size_t startAddress, std::span<u64 const> arguments);
};

} // namespace svm

#endif // SVM_VMIMPL_H_