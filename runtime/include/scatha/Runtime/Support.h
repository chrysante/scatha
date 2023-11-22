#ifndef SCATHA_RUNTIME_SUPPORT_H_
#define SCATHA_RUNTIME_SUPPORT_H_

#include <functional>
#include <string>
#include <tuple>
#include <vector>

#include <scatha/Sema/Fwd.h>
#include <svm/Fwd.h>

namespace scatha {

/// Struct member descriptor
struct StructMemberDesc {
    /// The name of the member as it will be accessible in source code
    std::string name;

    /// The type of the member
    sema::Type const* type;
};

/// Struct descriptor
struct StructDesc {
    /// The name of the struct as it will be accessible in source code
    std::string name;

    /// List of members
    std::vector<StructMemberDesc> members;
};

///
struct FuncDecl {
    std::string name;
    sema::Function const* function;
    size_t slot;
    size_t index;

    explicit operator bool() const { return function != nullptr; }
};

///
using InternalFuncPtr = void (*)(uint64_t* regptr,
                                 svm::VirtualMachine* vm,
                                 void* userptr);

///
template <typename F>
concept ValidFunction = requires(F&& f) { std::function{ std::move(f) }; } &&
                        (std::is_lvalue_reference_v<F&&> ||
                         std::is_empty_v<std::remove_reference_t<F>>);

namespace internal {

template <typename Function>
struct MakeImplAndUserPtr;

template <typename T>
struct Type {};

template <typename R, typename... Args>
struct MakeImplAndUserPtr<std::function<R(Args...)>> {
    template <typename F>
    static std::pair<InternalFuncPtr, void*> Impl(F&& f) {
        auto impl =
            [](uint64_t* regptr, svm::VirtualMachine* vm, void* userptr) {
            auto loadArgument =
                [&, regptr = regptr]<typename T>(Type<T>) mutable -> T {
                auto* p = regptr;
                regptr += sizeof(T) / 8 + (sizeof(T) % 8 != 0);
                alignas(T) char data[sizeof(T)];
                std::memcpy(data, p, sizeof(T));
                return *reinterpret_cast<T*>(&data);
            };
            auto args = std::tuple{ loadArgument(Type<Args>{})... };
            auto invoke = [&]() -> R {
                using FRaw = std::remove_reference_t<F>;
                if constexpr (std::is_empty_v<FRaw>) {
                    return std::apply(FRaw{}, args);
                }
                else {
                    return std::apply(*reinterpret_cast<FRaw*>(userptr), args);
                }
            };
            if constexpr (std::is_same_v<R, void>) {
                invoke();
            }
            else {
                auto ret = invoke();
                std::memcpy(regptr, &ret, sizeof(ret));
            }
        };
        void* userptr = [&]() -> void* {
            if constexpr (std::is_empty_v<std::remove_reference_t<F>>) {
                return nullptr;
            }
            else {
                static_assert(std::is_lvalue_reference_v<F>,
                              "The ValidFunction concept should ensure this");
                return const_cast<void*>(static_cast<void const volatile*>(&f));
            }
        }();
        return { impl, userptr };
    }
};

template <ValidFunction F>
std::pair<InternalFuncPtr, void*> makeImplAndUserPtr(F&& f) {
    return MakeImplAndUserPtr<decltype(std::function{ f })>::Impl(
        std::forward<F>(f));
}

template <typename Sig>
struct MakeFunction;

template <typename R, typename... Args>
struct MakeFunction<R(Args...)> {
    static auto Impl(auto* VM, size_t addr) {
        static constexpr auto toNumWords = [](size_t size) {
            return size / 8 + (size % 8 != 0);
        };
        static constexpr size_t ArgsNumWords =
            (0 + ... + toNumWords(sizeof(Args)));
        return [VM, addr](Args... args) -> R {
            auto virtArgs = [&] {
                size_t index = 0;
                std::array<uint64_t, ArgsNumWords> virtArgs;
                (
                    [&] {
                    std::memcpy(&virtArgs[index], &args, sizeof(Args));
                    index += toNumWords(sizeof(Args));
                    }(),
                    ...);
                return virtArgs;
            }();
            VM->execute(addr, virtArgs);
            auto* retData = VM->registerData().data();
            alignas(R) char ret[sizeof(R)];
            std::memcpy(ret, retData, sizeof(R));
            return reinterpret_cast<R&>(ret);
        };
    }
};

} // namespace internal

} // namespace scatha

#endif // SCATHA_RUNTIME_SUPPORT_H_
