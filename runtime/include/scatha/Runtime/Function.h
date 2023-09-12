#ifndef SCATHA_RUNTIME_FUNCTION_H_
#define SCATHA_RUNTIME_FUNCTION_H_

#include <concepts>
#include <string>

#include <scatha/Runtime/Common.h>
#include <svm/VirtualMachine.h>
#include <utl/type_traits.hpp>

namespace scatha {

/// Bind \p function to the external function position described by \p ID
/// Arguments will be loaded from and return value will stored to the respective
/// registers
///
template <typename Function>
void setForeignFunction(svm::VirtualMachine* VM,
                    ForeignFunctionID ID,
                    Function&& function);

/// Invoke the function at address \p funcAddress in the virtual machine \p VM
/// with arguments \p args... Behaviour is undefined, if the function is not
/// invokable with given arguments
template <typename R = void, typename... Args>
    requires scatha::Trivial<R> && (... && scatha::Trivial<Args>)
R run(svm::VirtualMachine* VM, size_t funcAddress, Args... args);

} // namespace scatha

// ===-----------------------------------------------------------------------===
// ===- Implementation ------------------------------------------------------===
// ===-----------------------------------------------------------------------===

template <typename F>
void scatha::setForeignFunction(svm::VirtualMachine* vm, ForeignFunctionID id, F&& f) {
    static_assert(
        std::is_empty_v<F> || std::is_lvalue_reference_v<F>,
        "If function type is not empty, it must be an l-value reference. It is "
        "also the users responsibility that that reference will be valid until "
        "the last call to this function");
    void* userdata = std::is_empty_v<F> ?
                         nullptr :
                         const_cast<void*>(reinterpret_cast<void const*>(&f));
    auto wrapFn = [](uint64_t* const r, svm::VirtualMachine*, void* userdata) {
        using namespace internal;
        using FT = utl::function_traits<F>;
        using R = typename FT::result_type;
        size_t index = 0;
        auto loadArg = [&]<size_t I>() {
            using T = typename FT::template argument<I>;
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
            store(r, std::apply(func, args));
        }
    };
    vm->setFunction(id.slot, id.index, { wrapFn, userdata });
}

template <typename R, typename... Args>
    requires scatha::Trivial<R> && (... && scatha::Trivial<Args>)
R scatha::run(svm::VirtualMachine* vm, size_t function, Args... args) {
    using namespace internal;
    static constexpr size_t argbufsize =
        (!sizeof...(args) + ... + ceildiv(sizeof args, 8));
    uint64_t argbuf[argbufsize]{};
    size_t index = 0;
    ((std::memcpy(argbuf + index, &args, sizeof args),
      index += ceildiv(sizeof args, 8)),
     ...);
    if constexpr (std::is_same_v<R, void>) {
        vm->execute(function, std::span<uint64_t const>(argbuf));
    }
    else {
        uint8_t retvalbuf[sizeof(R)]{};
        auto* r = vm->execute(function, std::span<uint64_t const>(argbuf));
        return *reinterpret_cast<R const*>(r);
    }
}

#endif // SCATHA_RUNTIME_FUNCTION_H_
