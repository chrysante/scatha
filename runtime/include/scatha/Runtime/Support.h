#ifndef SCATHA_RUNTIME_SUPPORT_H_
#define SCATHA_RUNTIME_SUPPORT_H_

#include <array>
#include <bit>
#include <cstring>
#include <functional>
#include <string>
#include <tuple>
#include <typeindex>
#include <variant>
#include <vector>

/// We don't include the Fwd.h header to not depend on the include paths of
/// Scatha and SVM
namespace scatha::sema {

class Function;
class Type;

} // namespace scatha::sema

namespace svm {

class VirtualMachine;

} // namespace svm

namespace scatha::internal {

enum class StaticCtorHelper : int;

inline int operator->*(StaticCtorHelper, auto f) {
    std::invoke(f);
    return 0;
}

} // namespace scatha::internal

/// Define this here so we don't have to `#include <scatha/Common/Base.h>`
#define SC_STATIC_CTOR_CONCAT_IMPL(a, b) a##b
#define SC_STATIC_CTOR_CONCAT(a, b)      SC_STATIC_CTOR_CONCAT_IMPL(a, b)

#define SC_STATIC_CONSTRUCTOR                                                  \
    static int SC_STATIC_CTOR_CONCAT(StaticCtor_, __LINE__) =                  \
        scatha::internal::StaticCtorHelper{}->*[]

namespace scatha {

using TypeID = std::variant<std::type_index, sema::Type const*>;

/// Struct member descriptor
struct StructMemberDesc {
    /// The name of the member as it will be accessible in source code
    std::string name;

    /// The type of the member
    TypeID type;
};

/// Struct descriptor
struct StructDesc {
    /// The name of the struct as it will be accessible in source code
    std::string name;

    /// List of members
    std::vector<StructMemberDesc> members;
};

/// VM function address
struct FuncAddress {
    size_t slot;
    size_t index;
};

///
struct FuncDecl {
    std::string name;
    sema::Function const* function;
    FuncAddress address;

    explicit operator bool() const { return function != nullptr; }
};

///
using InternalFuncPtr = void (*)(uint64_t* regptr,
                                 svm::VirtualMachine* vm,
                                 void* userptr);

///
template <typename F>
concept ValidFunction =
    /// The `std::function` deduction test guarantees that F has a distinct call
    /// signature
    requires(F&& f) { std::function{ std::move(f) }; } &&
    (std::is_lvalue_reference_v<F&&> ||
     std::is_empty_v<std::remove_reference_t<F>> ||
     !std::is_class_v<std::remove_reference_t<F>>);

namespace internal {

template <typename Function>
struct MakeImplAndUserPtr;

template <typename T>
struct Type {};

template <typename R, typename... Args>
struct MakeImplAndUserPtr<std::function<R(Args...)>> {
    template <typename F>
    static std::pair<InternalFuncPtr, void*> Impl(F&& f) {
        using FRaw = std::remove_reference_t<F>;
        auto impl =
            [](uint64_t* regptr, svm::VirtualMachine* vm, void* userptr) {
            auto loadArgument = [&, regptr]<typename T>(Type<T>) mutable {
                alignas(T) std::array<char, sizeof(T)> arg;
                std::memcpy(arg.data(), regptr, arg.size());
                regptr += sizeof(T) / 8 + (sizeof(T) % 8 != 0);
                return std::bit_cast<T>(arg);
            };
            auto invoke = [&] {
                if constexpr (std::is_empty_v<FRaw>) {
                    return std::invoke(FRaw{}, loadArgument(Type<Args>{})...);
                }
                else {
                    return std::invoke(*reinterpret_cast<FRaw*>(userptr),
                                       loadArgument(Type<Args>{})...);
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
            if constexpr (std::is_empty_v<FRaw>) {
                return nullptr;
            }
            else {
                static_assert(std::is_lvalue_reference_v<F>,
                              "The ValidFunction concept should ensure this");
                return const_cast<void*>(
                    reinterpret_cast<void const volatile*>(&f));
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

template <typename T>
inline constexpr size_t NumWords = sizeof(T) / 8 + (sizeof(T) % 8 != 0);

template <typename Sig>
struct MakeFunction;

template <typename R, typename... Args>
struct MakeFunction<R(Args...)> {
    static auto Impl(auto* VM, size_t addr) {
        static constexpr size_t ArgsNumWords = (0 + ... + NumWords<Args>);
        return [VM, addr](Args... args) -> R {
            auto virtArgs = [&] {
                size_t index = 0;
                std::array<uint64_t, ArgsNumWords> virtArgs{};
                // clang-format off
                ([&] {
                    std::memcpy(&virtArgs[index], &args, sizeof(Args));
                    index += NumWords<Args>;
                }(), ...); // clang-format on
                return virtArgs;
            }();
            auto* retData = VM->execute(addr, virtArgs);
            if constexpr (!std::is_same_v<R, void>) {
                alignas(R) std::array<char, sizeof(R)> ret;
                std::memcpy(ret.data(), retData, ret.size());
                return std::bit_cast<R>(ret);
            }
        };
    }
};

} // namespace internal

} // namespace scatha

#endif // SCATHA_RUNTIME_SUPPORT_H_
