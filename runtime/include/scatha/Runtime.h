#ifndef SCATHA_RUNTIME_RUNTIME_H_
#define SCATHA_RUNTIME_RUNTIME_H_

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <scatha/Runtime/Function.h>

namespace scatha {

template <typename T>
inline constexpr bool Trivial =
    std::is_trivially_copyable_v<T> || std::is_same_v<T, void>;

class Runtime {
public:
    explicit Runtime(svm::VirtualMachine* vm): vm(vm) {}

    void addFunction(RuntimeFunction f) { functions.push_back(std::move(f)); }

    void addSource(std::string source) { sources.push_back(std::move(source)); }

    void compile();

    template <typename R = void, typename... Args>
        requires Trivial<R> && (... && Trivial<Args>)
    R run(std::string_view function, Args... args);

private:
    void runImpl(std::string_view function,
                 std::span<uint64_t const> args,
                 std::span<uint8_t> retVal);

    std::vector<RuntimeFunction> functions;
    std::vector<std::string> sources;
    std::vector<uint8_t> binary;
    std::unordered_map<std::string, size_t> symbolTable;
    svm::VirtualMachine* vm;
};

template <typename R, typename... Args>
    requires Trivial<R> && (... && Trivial<Args>)
R Runtime::run(std::string_view function, Args... args) {
    static constexpr size_t argbufsize =
        (1 + ... + internal::ceildiv(sizeof(Args), 8));
    uint64_t argbuf[argbufsize]{};
    size_t index = 0;
    (
        [&] {
        std::memcpy(argbuf + index, &args, sizeof(args));
        index += internal::ceildiv(sizeof(Args), 8);
        }(),
        ...);
    if constexpr (std::is_same_v<R, void>) {
        runImpl(function, argbuf, {});
    }
    else {
        alignas(R) uint8_t retvalBuffer[sizeof(R)];
        runImpl(function, argbuf, retvalBuffer);
        return *reinterpret_cast<R*>(&retvalBuffer);
    }
}

} // namespace scatha

#endif // SCATHA_RUNTIME_RUNTIME_H_
