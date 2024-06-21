#include "Assembly/Assembler.h"

#include <optional>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <svm/Builtin.h>
#include <svm/Program.h>
#include <utl/dynamic_library.hpp>
#include <utl/hashtable.hpp>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include "Assembly/AsmWriter.h"
#include "Common/Builtin.h"

using namespace scatha;
using namespace Asm;
using namespace ranges::views;

namespace {

enum class FFIKind { Builtin, Foreign };

struct FFIAddress {
    FFIKind kind;
    size_t index;

    uint16_t toMachineRepr() const { return utl::narrow_cast<uint16_t>(index); }
};

/// Represents a foreign function declaration
struct FFIDecl {
    ForeignFunctionInterface interface;
    FFIAddress address;
};

/// List of foreign functions in one library
struct FFIList {
    explicit FFIList(std::string name): libName(std::move(name)) {}

    std::string libName;
    std::vector<FFIDecl> functions;
};

struct Linker: AsmWriter {
    /// User provided options
    LinkerOptions const& options;

    /// List of supplied library file paths
    std::span<ForeignLibraryDecl const> foreignLibs;

    /// Assembler output
    std::span<std::pair<size_t, ForeignFunctionInterface> const>
        unresolvedSymbols;

    /// To be filled by this pass
    std::vector<std::string> missingSymbols;

    Linker(LinkerOptions const& options, std::vector<uint8_t>& binary,
           std::span<ForeignLibraryDecl const> foreignLibs,
           std::span<std::pair<size_t, ForeignFunctionInterface> const>
               unresolvedSymbols):
        AsmWriter(binary),
        options(options),
        foreignLibs(foreignLibs),
        unresolvedSymbols(unresolvedSymbols) {}

    Expected<void, LinkerError> run();

    /// Searches the shared object \p lib for functions in \p foreignFunctions
    /// Resolved functions are erased from \p foreignFunctions and added to
    /// \p ffiList
    void resolveInObject(utl::dynamic_library const& lib, FFIList& ffiList,
                         utl::vector<FFIDecl>& foreignFunctions);

    std::vector<FFIList> search();

    void link(std::span<FFIList const> ffiLists);

    void putFFIType(FFIType const* type);

    ///
    void replaceSection(size_t pos, size_t size, std::span<u8 const> data) {
        if (size <= data.size()) {
            std::memcpy(&binary[pos], data.data(), size);
            binary.insert(binary.begin() + static_cast<ssize_t>(pos + size),
                          data.begin() + static_cast<ssize_t>(size),
                          data.end());
        }
        else {
            std::memcpy(&binary[pos], data.data(), data.size());
            binary.erase(binary.begin() +
                             static_cast<ssize_t>(pos + data.size()),
                         binary.begin() + static_cast<ssize_t>(pos + size));
        }
    }

    template <typename T>
    void replaceSection(size_t pos, size_t size, T const& value) {
        std::span<u8 const> span(reinterpret_cast<u8 const*>(&value),
                                 sizeof(value));
        replaceSection(pos, size, span);
    }
};

///
struct AddressFactory {
    FFIAddress operator()(std::string_view name) {
        if (name.starts_with("__builtin_")) {
            auto index = getBuiltinIndex(name);
            SC_ASSERT(index, "Undefined builtin");
            return { FFIKind::Builtin, *index };
        }
        return { FFIKind::Foreign, FFIndex++ };
    }

    size_t FFIndex = 0;
};

} // namespace

Expected<void, LinkerError> Asm::link(
    LinkerOptions options, std::vector<uint8_t>& binary,
    std::span<ForeignLibraryDecl const> foreignLibs,
    std::span<std::pair<size_t, ForeignFunctionInterface> const>
        unresolvedSymbols) {
    SC_ASSERT(binary.size() >= sizeof(svm::ProgramHeader),
              "Binary must at least contain a header");
    Linker linker(options, binary, foreignLibs, unresolvedSymbols);
    auto result = linker.run();
    /// Update binary size because we placed the dynamic link section in the
    /// back
    auto& header = *reinterpret_cast<svm::ProgramHeader*>(binary.data());
    header.size = sizeof(svm::ProgramHeader) + binary.size();
    return result;
}

Expected<void, LinkerError> Linker::run() {
    auto ffiLists = search();
    if (!missingSymbols.empty()) {
        return LinkerError{ std::move(missingSymbols) };
    }
    link(ffiLists);
    return {};
}

void Linker::resolveInObject(utl::dynamic_library const& lib, FFIList& ffiList,
                             utl::vector<FFIDecl>& foreignFunctions) {
    for (auto itr = foreignFunctions.begin(); itr != foreignFunctions.end();) {
        auto& function = *itr;
        std::string err;
        if (lib.resolve(function.interface.name(), &err)) {
            ffiList.functions.push_back(function);
            itr = foreignFunctions.erase(itr);
        }
        else {
            ++itr;
        }
    }
}

std::vector<FFIList> Linker::search() {
    utl::small_vector<FFIDecl> foreignFunctions;
    auto makeAddress = AddressFactory{};
    /// Gather names and replace with addresses
    for (auto [symPos, interface]: unresolvedSymbols | reverse) {
        FFIAddress addr = makeAddress(interface.name());
        SC_ASSERT(
            binary[symPos] == 0xFF && binary[symPos + 1] == 0xFF,
            "Two bytes shall be placeholder that we will use for the index");
        uint16_t machineAddr = addr.toMachineRepr();
        std::memcpy(&binary[symPos], &machineAddr, sizeof machineAddr);
        if (addr.kind == FFIKind::Foreign) {
            foreignFunctions.push_back({ interface, addr });
        }
    }
/// Find names in foreign libraries
#ifndef _MSC_VER
    auto ffiLists = foreignLibs |
                    transform([](auto& lib) { return FFIList(lib.name()); }) |
                    ranges::to<std::vector>;
#else
    std::vector<FFIList> ffiLists;
    ffiLists.reserve(foreignLibs.size());
    std::transform(foreignLibs.begin(), foreignLibs.end(),
                   std::back_inserter(ffiLists),
                   [](auto& lib) { return FFIList(lib.name()); });
#endif
    for (auto [libIndex, libDecl]: foreignLibs | enumerate) {
        SC_ASSERT(libDecl.resolvedPath(),
                  "Tried to link symbol in unresolved library");
        utl::dynamic_library lib(libDecl.resolvedPath().value().string(),
                                 utl::dynamic_load_mode::lazy);
        resolveInObject(lib, ffiLists[libIndex], foreignFunctions);
    }
    if (!foreignFunctions.empty() && options.searchHost) {
        ffiLists.push_back(FFIList("")); /// Empty name means search host
        resolveInObject(utl::dynamic_library::global(
                            utl::dynamic_load_mode::lazy),
                        ffiLists.back(), foreignFunctions);
    }
    missingSymbols = foreignFunctions | transform(&FFIDecl::interface) |
                     transform(&ForeignFunctionInterface::name) |
                     ranges::to<std::vector>;
    return ffiLists;
}

void Linker::link(std::span<FFIList const> ffiLists) {
    setPosition(binary.size());
    /// Number of foreign libraries
    put<u32>(ffiLists.size());
    for (auto& ffiList: ffiLists) {
        /// Null-terminated string denoting library name
        putNullTerm(ffiList.libName);
        /// Number of foreign function declarations
        put<u32>(ffiList.functions.size());
        for (auto& [interface, address]: ffiList.functions) {
            putNullTerm(interface.name());
            put<u8>(interface.argumentTypes().size());
            for (auto* type: interface.argumentTypes()) {
                putFFIType(type);
            }
            putFFIType(interface.returnType());
            put<u32>(address.index);
        }
    }
}

void Linker::putFFIType(FFIType const* type) {
    put<u8>(static_cast<uint64_t>(type->kind()));
    if (type->isTrivial()) {
        return;
    }
    auto* s = dynamic_cast<FFIStructType const*>(type);
    SC_ASSERT(s, "");
    put<u16>(utl::narrow_cast<uint16_t>(s->elements().size()));
    for (auto* elem: s->elements()) {
        putFFIType(elem);
    }
}
