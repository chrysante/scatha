#ifndef SCATHA_RUNTIME_FUNCTION_H_
#define SCATHA_RUNTIME_FUNCTION_H_

#include <concepts>
#include <string>

#include <scatha/Runtime/Common.h>
#include <svm/VirtualMachine.h>
#include <utl/type_traits.hpp>

namespace svm {

class VirtualMachine;

} // namespace svm

namespace scatha {

template <typename T>
inline constexpr bool Trivial =
    std::is_trivially_copyable_v<T> || std::is_same_v<T, void>;

template <typename F>
void setExtFunction(svm::VirtualMachine* vm, ExtFunctionID id, F&& f) {
    static_assert(
        std::is_empty_v<F> || std::is_lvalue_reference_v<F>,
        "If function type is not empty, it must be an l-value reference that "
        "will be valid until the last call to this function");
    void* userdata              = std::is_empty_v<F> ?
                                      nullptr :
                                      const_cast<void*>(reinterpret_cast<void const*>(&f));
    svm::ExternalFunction extFn = {
        [](uint64_t* r, svm::VirtualMachine*, void* userdata) {
        using namespace internal;
        using FT     = utl::function_traits<F>;
        using R      = typename FT::result_type;
        size_t index = 0;
        auto loadArg = [&]<size_t I>() {
            using T  = typename FT::template argument<I>;
            T result = load<T>(r + index);
            index += ceildiv(sizeof(T), 8);
            return result;
        };
        auto args = [&]<size_t... I>(std::index_sequence<I...>) {
            return std::tuple{ loadArg.template operator()<I>()... };
        }(std::make_index_sequence<FT::argument_count>());
        F&& func = [&]() -> decltype(auto) {
            if constexpr (std::is_empty_v<F>) {
                return F{};
            }
            else {
                return *static_cast<std::remove_reference_t<F>*>(userdata);
            }
        }();
        if constexpr (std::is_same_v<R, void>) {
            std::apply(func, args);
        }
        else {
            R retval = std::apply(func, args);
            store(r, retval);
        }
        },
        userdata };
    vm->setFunction(id.slot, id.index, extFn);
}

inline void run(svm::VirtualMachine* vm,
                size_t function,
                std::span<uint64_t const> args,
                std::span<uint8_t> retval) {
    auto* r = vm->execute(function, args);
    std::memcpy(retval.data(), r, retval.size());
}

template <typename R = void, typename... Args>
    requires Trivial<R> && (... && Trivial<Args>)
R run(svm::VirtualMachine* vm, size_t function, Args... args) {
    using namespace internal;
    static constexpr size_t argbufsize =
        (!sizeof...(args) + ... + ceildiv(sizeof args, 8));
    uint64_t argbuf[argbufsize]{};
    size_t index = 0;
    ((std::memcpy(argbuf + index, &args, sizeof args),
      index += ceildiv(sizeof args, 8)),
     ...);
    if constexpr (std::is_same_v<R, void>) {
        run(vm,
            function,
            std::span<uint64_t const>(argbuf),
            std::span<uint8_t>{});
    }
    else {
        uint8_t retvalbuf[sizeof(R)]{};
        run(vm,
            function,
            std::span<uint64_t const>(argbuf),
            std::span<uint8_t>(retvalbuf));
        return reinterpret_cast<R&>(retvalbuf);
    }
}

} // namespace scatha

#endif // SCATHA_RUNTIME_FUNCTION_H_
