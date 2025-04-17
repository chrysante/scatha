#include "VirtualMachine.h"

#include <bit>
#include <iostream>

#include <ffi.h>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scbinutil/ProgramView.h>
#include <utl/dynamic_library.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "BuiltinInternal.h"
#include "Common.h"
#include "Errors.h"
#include "Memory.h"
#include "VMImpl.h"

using namespace svm;
using namespace ranges::views;

VirtualMachine::VirtualMachine():
    VirtualMachine(DefaultRegisterCount, DefaultStackSize) {}

VirtualMachine::VirtualMachine(size_t numRegisters, size_t stackSize) {
    impl = std::make_unique<VMImpl>();
    impl->parent = this;
    impl->registers.resize(numRegisters);
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

/// This function is a copy of the same function in "SymbolTable.cc"
static std::string toForeignLibName(std::string_view fullname) {
    /// TODO: Make portable
    /// This is the MacOS convention, need to add linux and windows conventions
    /// for portability
#if defined(__APPLE__)
    std::filesystem::path path(fullname);
    auto name = path.filename().string();
    path.replace_filename(utl::strcat("lib", name, ".dylib"));
#elif defined(_WIN32)
    std::filesystem::path path(fullname);
    auto name = path.filename().string();
    path.replace_filename(utl::strcat(name, ".dll"));
#else
#error Unknown OS
#endif
    return path.string();
}

static utl::dynamic_library loadLibrary(std::filesystem::path const& libdir,
                                        std::string_view name) {
    /// Empty name means search host
    if (name.empty()) {
        return utl::dynamic_library::global(utl::dynamic_load_mode::lazy);
    }
    auto libname = toForeignLibName(name);
    try {
        return utl::dynamic_library((libdir / libname).string());
    }
    catch (std::exception const&) {
        /// Nothing, try again unscoped
    }
    return utl::dynamic_library(libname);
}

static ffi_type* toLibFFI(scbinutil::FFIType const* type);

ffi_type svm::ArrayPtrType = [] {
    ffi_type result;
    result.size = 0;
    result.alignment = 0;
    result.type = FFI_TYPE_STRUCT;
    static ffi_type* elems[] = { &ffi_type_pointer, &ffi_type_sint64, nullptr };
    result.elements = elems;
    return result;
}();

static ffi_type* mapFFIStructType(scbinutil::FFIStructType const* type) {
    struct LibFFITypeWrapper {
        std::vector<ffi_type*> elems;
        ffi_type type;
    };
    static utl::node_hashmap<scbinutil::FFIStructType const*, LibFFITypeWrapper>
        map;
    if (auto itr = map.find(type); itr != map.end()) {
        return &itr->second.type;
    }
    std::vector<ffi_type*> elements;
    elements.reserve(type->elements().size() + 1);
    std::for_each(type->elements().begin(), type->elements().end(),
                  [&](auto* elem) { elements.push_back(toLibFFI(elem)); });
    elements.push_back(nullptr);
    LibFFITypeWrapper wrapper = { .elems = std::move(elements) };
    wrapper.type.size = 0;
    wrapper.type.alignment = 0;
    wrapper.type.type = FFI_TYPE_STRUCT;
    wrapper.type.elements = wrapper.elems.data();
    auto [itr, success] = map.insert({ type, std::move(wrapper) });
    assert(success);
    return &itr->second.type;
}

static ffi_type* toLibFFI(scbinutil::FFIType const* type) {
    using enum scbinutil::FFIType::Kind;
    switch (type->kind()) {
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
    case Struct:
        return mapFFIStructType(
            static_cast<scbinutil::FFIStructType const*>(type));
    }
    return nullptr;
}

static bool initForeignFunction(scbinutil::FFIDecl const& decl,
                                ForeignFunction& F) {
#ifndef _MSC_VER
    F.name = decl.name;
    F.funcPtr = (void (*)())decl.ptr;
    F.returnType = toLibFFI(decl.returnType);
    F.argumentTypes.clear();
    std::for_each(decl.argumentTypes.begin(), decl.argumentTypes.end(),
                  [&](auto* type) {
        F.argumentTypes.push_back(toLibFFI(type));
    });
    F.arguments.resize(F.argumentTypes.size());
    return ffi_prep_cif(&F.callInterface, FFI_DEFAULT_ABI,
                        utl::narrow_cast<unsigned>(F.arguments.size()),
                        F.returnType, F.argumentTypes.data()) == FFI_OK;
#else
    throwError<FFIError>(FFIError::FailedToInit, F.name);
#endif
}

static void loadForeignFunctions(
    VirtualMachine* vm, std::span<scbinutil::FFILibDecl const> libDecls) {
    std::vector<scbinutil::FFIDecl> fnDecls;
    for (auto& libDecl: libDecls) {
        auto lib = loadLibrary(vm->impl->libdir, libDecl.name);
        for (auto FFI: libDecl.funcDecls) {
            FFI.ptr = lib.resolve(FFI.name);
            fnDecls.push_back(FFI);
        }
        vm->impl->dylibs.push_back(std::move(lib));
    }
    ranges::sort(fnDecls, ranges::less{}, &scbinutil::FFIDecl::index);
    /// We clear before we resize because `ForeignFunction` traps on copy
    /// construction
    vm->impl->foreignFunctionTable.clear();
    vm->impl->foreignFunctionTable.resize(fnDecls.size());
    for (auto [decl, F]: zip(fnDecls, vm->impl->foreignFunctionTable)) {
        if (!initForeignFunction(decl, F)) {
            throwError<FFIError>(FFIError::FailedToInit, decl.name);
        }
    }
}

void VirtualMachine::loadBinary(u8 const* progData) {
    scbinutil::ProgramView program(progData);
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
    if (program.startAddress != scbinutil::InvalidAddress) {
        impl->startAddress = program.startAddress;
    }
    loadForeignFunctions(this, program.libDecls);
    reset();
}

u64 const* VirtualMachine::execute(std::span<u64 const> arguments) {
    if (!impl->startAddress.has_value()) {
        throwError<NoStartAddress>();
    }
    return execute(*impl->startAddress, arguments);
}

u64 const* VirtualMachine::execute(size_t startAddress,
                                   std::span<u64 const> arguments) {
    impl->beginExecution(startAddress, arguments);
    impl->execute<ExecutionMode::Default>();
    return impl->endExecution();
}

u64 const* VirtualMachine::executeNoJumpThread(std::span<u64 const> arguments) {
    if (!impl->startAddress.has_value()) {
        throwError<NoStartAddress>();
    }
    return executeNoJumpThread(*impl->startAddress, arguments);
}

u64 const* VirtualMachine::executeNoJumpThread(size_t startAddress,
                                               std::span<u64 const> arguments) {
    impl->beginExecution(startAddress, arguments);
    impl->executeNoJumpThread<ExecutionMode::Default>();
    return impl->endExecution();
}

void VirtualMachine::beginExecution(std::span<u64 const> arguments) {
    if (!impl->startAddress.has_value()) throwError<NoStartAddress>();
    beginExecution(*impl->startAddress, arguments);
}

void VirtualMachine::beginExecution(size_t startAddress,
                                    std::span<u64 const> arguments) {
    return impl->beginExecution(startAddress, arguments);
}

bool VirtualMachine::running() const { return impl->running(); }

void VirtualMachine::stepExecution() { impl->stepExecution(); }

void VirtualMachine::executeInterruptible() {
    impl->execute<ExecutionMode::Interruptible>();
}

void VirtualMachine::interruptExecution() { impl->interruptExecution(); }

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

void VirtualMachine::deallocateMemory(VirtualPointer ptr, size_t size,
                                      size_t align) {
    impl->memory.deallocate(ptr, size, align);
}

VirtualPointer VirtualMachine::mapMemory(void* p, size_t size) {
    return impl->memory.map(p, size);
}

void VirtualMachine::unmapMemory(size_t slotIndex) {
    return impl->memory.unmap(slotIndex);
}

void VirtualMachine::unmapMemory(VirtualPointer p) {
    return impl->memory.unmap(p.slotIndex);
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
