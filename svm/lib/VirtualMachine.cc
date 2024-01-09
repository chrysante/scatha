#include "VirtualMachine.h"

#include <bit>
#include <iostream>

#include <ffi.h>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/dynamic_library.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "BuiltinInternal.h"
#include "Memory.h"
#include "Program.h"
#include "VMImpl.h"

using namespace svm;
using namespace ranges::views;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize) {
    impl = std::make_unique<VMImpl>();
    impl->parent = this;
    impl->registers.resize(numRegisters, utl::no_init);
    impl->stackSize = stackSize;
    impl->builtinFunctionTable = makeBuiltinTable();
}

VirtualMachine::VirtualMachine(VirtualMachine&& rhs) noexcept:
    impl(std::move(rhs.impl)) {
    impl->parent = this;
}

VirtualMachine& VirtualMachine::operator=(VirtualMachine&& rhs) noexcept {
    impl = std::move(rhs.impl);
    impl->parent = this;
    return *this;
}

VirtualMachine::~VirtualMachine() = default;

static std::string toLibName(std::string_view name) {
    /// TODO: Make portable
    /// This is the MacOS convention, need to add linux and windows conventions
    /// for portability
    return utl::strcat("lib", name, ".dylib");
}

static utl::dynamic_library loadLibrary(std::filesystem::path const& libdir,
                                        std::string_view name) {
    return utl::dynamic_library(libdir / toLibName(name));
}

static ffi_type* toLibFFI(FFIType type) {
    using enum FFIType;
    switch (type) {
    case Void:
        return &ffi_type_void;
    case Int8:
        return &ffi_type_sint8;
    case Int16:
        return &ffi_type_sint16;
    case Int32:
        return &ffi_type_sint32;
    case Int64:
        return &ffi_type_sint64;
    case Float:
        return &ffi_type_float;
    case Double:
        return &ffi_type_double;
    case Pointer:
        return &ffi_type_pointer;
    }
}

static bool initForeignFunction(FFIDecl const& decl, ForeignFunction& F) {
    F.name = decl.name;
    F.funcPtr = (void (*)())decl.ptr;
    F.argumentTypes = decl.argumentTypes | transform(toLibFFI) |
                      ranges::to<utl::small_vector<ffi_type*>>;
    F.arguments.resize(F.argumentTypes.size());
    return ffi_prep_cif(&F.callInterface,
                        FFI_DEFAULT_ABI,
                        utl::narrow_cast<int>(F.arguments.size()),
                        toLibFFI(decl.returnType),
                        F.argumentTypes.data()) == FFI_OK;
}

static void loadForeignFunctions(VirtualMachine* vm,
                                 std::span<FFILibDecl const> libDecls) {
    std::vector<FFIDecl> fnDecls;
    for (auto& libDecl: libDecls) {
        auto lib = loadLibrary(vm->impl->libdir, libDecl.name);
        for (auto FFI: libDecl.funcDecls) {
            FFI.ptr = lib.resolve(FFI.name);
            fnDecls.push_back(FFI);
        }
        vm->impl->dylibs.push_back(std::move(lib));
    }
    assert(ranges::is_sorted(fnDecls, ranges::less{}, &FFIDecl::index));
    vm->impl->foreignFunctionTable.resize(fnDecls.size());
    for (auto [decl, F]: zip(fnDecls, vm->impl->foreignFunctionTable)) {
        if (!initForeignFunction(decl, F)) {
            throwError<FFIError>(FFIError::FailedToInit, decl.name);
        }
    }
}

void VirtualMachine::loadBinary(u8 const* progData) {
    ProgramView program(progData);
    size_t binSize = utl::round_up(program.binary.size(), 16);
    impl->memory.resizeStaticSlot(binSize + impl->stackSize);
    auto staticData = VirtualMemory::MakeStaticDataPointer(0);
    u8* rawStaticData = &impl->memory.derefAs<u8>(staticData, 0);
    assert(reinterpret_cast<uintptr_t>(rawStaticData) % 16 == 0 &&
           "We just hope this is correctly aligned, if not we'll have to "
           "figure something out");
    std::memcpy(rawStaticData, program.binary.data(), program.binary.size());

    impl->binary = rawStaticData;
    impl->binarySize = binSize;
    impl->programBreak = impl->binary + program.binary.size();
    impl->startAddress = program.startAddress;
    loadForeignFunctions(this, program.libDecls);
    reset();
}

u64 const* VirtualMachine::execute(std::span<u64 const> arguments) {
    return execute(impl->startAddress, arguments);
}

u64 const* VirtualMachine::execute(size_t startAddress,
                                   std::span<u64 const> arguments) {
    return impl->execute(startAddress, arguments);
}

void VirtualMachine::beginExecution(std::span<u64 const> arguments) {
    return impl->beginExecution(impl->startAddress, arguments);
}

void VirtualMachine::beginExecution(size_t startAddress,
                                    std::span<u64 const> arguments) {
    return impl->beginExecution(startAddress, arguments);
}

bool VirtualMachine::running() const { return impl->running(); }

void VirtualMachine::stepExecution() { impl->stepExecution(); }

u64 const* VirtualMachine::endExecution() { return impl->endExecution(); }

void VirtualMachine::reset() {
    /// Clobber registers
    std::memset(impl->registers.data(), 0xcf, impl->registers.size() * 8);
    ///
    impl->execFrames.clear();
    impl->currentFrame = impl->execFrames.push(
        { .regPtr = impl->registers.data() - MaxCallframeRegisterCount,
          .bottomReg = impl->registers.data() - MaxCallframeRegisterCount,
          .iptr = nullptr,
          .stackPtr = VirtualMemory::MakeStaticDataPointer(impl->binarySize) });
}

size_t VirtualMachine::instructionPointerOffset() const {
    return impl->instructionPointerOffset();
}

void VirtualMachine::setInstructionPointerOffset(size_t offset) {
    impl->setInstructionPointerOffset(offset);
}

std::span<u64 const> VirtualMachine::registerData() const {
    return impl->registers;
}

u64 VirtualMachine::getRegister(size_t index) const {
    return impl->registers[index];
}

std::span<u8 const> VirtualMachine::stackData() const {
    auto* raw = impl->binary + impl->binarySize;
    return std::span(raw, impl->stackSize);
}

CompareFlags VirtualMachine::getCompareFlags() const { return impl->cmpFlags; }

ExecutionFrame VirtualMachine::getCurrentExecFrame() const {
    return impl->currentFrame;
}

void VirtualMachine::printRegisters(size_t n) const {
    for (size_t i = 0; i < n; ++i) {
        std::cout << "%" << i << ": " << std::hex
                  << impl->currentFrame.regPtr[i] << std::endl;
    }
}

VirtualPointer VirtualMachine::allocateStackMemory(size_t numBytes,
                                                   size_t align) {
    alignTo(impl->currentFrame.stackPtr, align);
    auto result = impl->currentFrame.stackPtr;
    impl->currentFrame.stackPtr += numBytes;
    alignTo(impl->currentFrame.stackPtr, 8);
    return result;
}

VirtualPointer VirtualMachine::allocateMemory(size_t size, size_t align) {
    return impl->memory.allocate(size, align);
}

void VirtualMachine::deallocateMemory(VirtualPointer ptr,
                                      size_t size,
                                      size_t align) {
    impl->memory.deallocate(ptr, size, align);
}

ptrdiff_t VirtualMachine::validPtrRange(VirtualPointer ptr) const {
    return impl->memory.validRange(ptr);
}

void* VirtualMachine::derefPointer(VirtualPointer ptr, size_t numBytes) const {
    return impl->memory.dereference(ptr, numBytes);
}

void VirtualMachine::setIOStreams(std::istream* in, std::ostream* out) {
    if (in) {
        impl->istream = in;
    }
    if (out) {
        impl->ostream = out;
    }
}

std::istream& VirtualMachine::istream() const { return *impl->istream; }

std::ostream& VirtualMachine::ostream() const { return *impl->ostream; }

std::string VirtualMachine::getBuiltinFunctionName(size_t index) const {
    if (index >= impl->builtinFunctionTable.size()) {
        return "<invalid-builtin>";
    }
    return impl->builtinFunctionTable[index].name();
}

std::string VirtualMachine::getForeignFunctionName(size_t index) const {
    if (index >= impl->foreignFunctionTable.size()) {
        return "<invalid-ffi>";
    }
    return impl->foreignFunctionTable[index].name;
}

void VirtualMachine::setLibdir(std::filesystem::path libdir) {
    impl->libdir = std::move(libdir);
}

VMImpl::VMImpl(): istream(&std::cin), ostream(&std::cout) {}
