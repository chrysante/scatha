#include <svm/Util.h>

#include <fstream>
#include <string>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <scbinutil/Utils.h>
#include <utl/strcat.hpp>
#include <utl/vector.hpp>

#include <svm/VirtualMachine.h>

using namespace svm;

std::vector<uint8_t> svm::readBinaryFromFile(std::string_view path) {
    std::fstream file(std::string(path), std::ios::in);
    if (!file) {
        throw std::runtime_error(utl::strcat("Failed to open program: \"", path,
                                             "\". File does not exist."));
    }
    file.seekg(0, std::ios::end);
    auto const filesize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<u8> data(static_cast<size_t>(filesize));
    file.read(reinterpret_cast<char*>(data.data()), filesize);
    return scbinutil::seekBinary(data) | ranges::to<std::vector>;
}

namespace {

struct ArgRange {
    size_t offset, size;
};

struct Arguments {
    utl::small_vector<ArgRange> pointers;
    utl::vector<u8> data;
};

} // namespace

static Arguments generateArguments(std::span<std::string const> args) {
    Arguments result;
    for (auto& arg: args) {
        result.pointers.push_back({ result.data.size(), arg.size() });
        ranges::copy(arg, std::back_inserter(result.data));
    }
    return result;
}

std::array<u64, 2> svm::setupArguments(VirtualMachine& vm,
                                       std::span<std::string const> args) {
    auto arguments = generateArguments(args);
    size_t argPointersSize = arguments.pointers.size() * 16;
    size_t stringDataSize = arguments.data.size();
    size_t totalArgSize = argPointersSize + stringDataSize;
    VirtualPointer argStackData = vm.allocateStackMemory(totalArgSize, 8);
    struct StringPointer {
        VirtualPointer ptr;
        uint64_t size;
    };
    auto argPointers = arguments.pointers |
                       ranges::views::transform([&](auto arg) {
        return StringPointer{ argStackData + argPointersSize + arg.offset,
                              arg.size };
    }) | ranges::to<utl::small_vector<StringPointer>>;
    std::memcpy(vm.derefPointer(argStackData, argPointersSize),
                argPointers.data(), argPointersSize);
    std::memcpy(vm.derefPointer(argStackData + argPointersSize, stringDataSize),
                arguments.data.data(), stringDataSize);
    return { std::bit_cast<u64>(argStackData), argPointers.size() };
}
