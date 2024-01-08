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

struct FFIAddress {
    size_t slot;
    size_t index;

    std::array<uint8_t, 3> toMachineRepr() const {
        std::array<uint8_t, 3> res;
        res[0] = utl::narrow_cast<uint8_t>(slot);
        auto idx = utl::narrow_cast<uint16_t>(index);
        std::memcpy(&res[1], &idx, 2);
        return res;
    }
};

/// Represents a foreign function declaration
struct FFIDecl {
    ForeignFunctionInterface interface;
    FFIAddress address;
};

/// List of foreign functions in one library
struct FFIList {
    FFIList(std::string libName = ""): libName(std::move(libName)) {}

    std::string libName;
    std::vector<FFIDecl> functions;
};

struct Linker: AsmWriter {
    /// List of supplied library file paths
    std::span<std::filesystem::path const> foreignLibs;

    /// Assembler output
    std::span<std::pair<size_t, ForeignFunctionInterface> const>
        unresolvedSymbols;

    /// To be filled by this pass
    std::vector<std::string> missingSymbols;

    Linker(std::vector<uint8_t>& binary,
           std::span<std::filesystem::path const> foreignLibs,
           std::span<std::pair<size_t, ForeignFunctionInterface> const>
               unresolvedSymbols):
        AsmWriter(binary),
        foreignLibs(foreignLibs),
        unresolvedSymbols(unresolvedSymbols) {}

    Expected<void, LinkerError> run();

    std::vector<FFIList> search();

    void link(std::span<FFIList const> ffiLists);

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
            if (auto index = getBuiltinIndex(name)) {
                return { svm::BuiltinFunctionSlot, *index };
            }
        }
        static constexpr size_t FFSlot = 2;
        return { FFSlot, FFIndex++ };
    }

    size_t FFIndex = 0;
};

} // namespace

Expected<void, LinkerError> Asm::link(
    std::vector<uint8_t>& binary,
    std::span<std::filesystem::path const> foreignLibs,
    std::span<std::pair<size_t, ForeignFunctionInterface> const>
        unresolvedSymbols) {
    SC_ASSERT(binary.size() >= sizeof(svm::ProgramHeader),
              "Binary must at least contain a header");
    Linker linker(binary, foreignLibs, unresolvedSymbols);
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

static std::string libpathToName(std::filesystem::path const& path) {
    auto file = path.filename();
    file.replace_extension();
    auto name = file.string();
    /// Erase prefixed "lib"
    SC_ASSERT(name.starts_with("lib"), "");
    name.erase(name.begin(), name.begin() + 3);
    return name;
}

std::vector<FFIList> Linker::search() {
    utl::small_vector<FFIDecl> foreignFunctions;
    auto makeAddress = AddressFactory{};
    /// Gather names and replace with addresses
    for (auto [symPos, interface]: unresolvedSymbols | reverse) {
        FFIAddress addr = makeAddress(interface.name());
        SC_ASSERT(binary[symPos] == 0xFF && binary[symPos + 1] == 0xFF &&
                      binary[symPos + 2] == 0xFF,
                  "");
        auto machineAddr = addr.toMachineRepr();
        std::memcpy(&binary[symPos], &machineAddr, 3);
        if (addr.slot != svm::BuiltinFunctionSlot) {
            foreignFunctions.push_back({ interface, addr });
        }
    }

    /// Find names in foreign libraries
    auto ffiLists = foreignLibs | transform(libpathToName) |
                    ranges::to<std::vector<FFIList>>;
    for (auto [libIndex, path]: foreignLibs | enumerate) {
        utl::dynamic_library lib(path, utl::dynamic_load_mode::lazy);
        for (auto itr = foreignFunctions.begin();
             itr != foreignFunctions.end();)
        {
            auto& function = *itr;
            std::string_view err;
            if (lib.resolve(function.interface.name(), &err)) {
                ffiLists[libIndex].functions.push_back(function);
                itr = foreignFunctions.erase(itr);
            }
            else {
                ++itr;
            }
        }
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
            for (auto type: interface.argumentTypes()) {
                put<u8>((uint64_t)type);
            }
            put<u8>((uint64_t)interface.returnType());
            put<u32>(address.slot);
            put<u32>(address.index);
        }
    }
}
