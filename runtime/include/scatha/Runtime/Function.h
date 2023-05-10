#ifndef SCATHA_RUNTIME_FUNCTION_H_
#define SCATHA_RUNTIME_FUNCTION_H_

#include <concepts>
#include <string>

#include <utl/type_traits.hpp>

namespace svm {

class VirtualMachine;

} // namespace svm

namespace scatha {

class RuntimeFunction {
public:
    struct Signature {
        void const* returnType;
        std::vector<void const*> argTypes;
    };

    using PtrType        = void (*)(uint64_t*, svm::VirtualMachine*, void*);
    using SigBuilderType = Signature (*)(void*);

    template <typename F>
    RuntimeFunction(std::string name, F& func):
        RuntimeFunction(make(std::move(name), func)) {}

    /// The name of this function as at will be called in source code
    std::string const& name() const { return _name; }

    /// The constructed function pointer
    PtrType pointer() const { return _fnPtr; }

    /// The resolved user data
    void* userData() const { return _userData; }

    ///
    Signature buildSig(void* symTable) const { return _sigBuilder(symTable); }

private:
    explicit RuntimeFunction(std::string name,
                             PtrType pointer,
                             void* userData,
                             SigBuilderType sigBuilder):
        _name(std::move(name)),
        _fnPtr(pointer),
        _userData(userData),
        _sigBuilder(sigBuilder) {}

    template <typename F>
    static RuntimeFunction make(std::string name, F& func);

    std::string _name;
    PtrType _fnPtr;
    void* _userData;
    SigBuilderType _sigBuilder;
};

/// Helper class to access arguments and return values from host functions
class Loader {
public:
    explicit Loader(uint64_t* regPtr): _regPtr(regPtr) {}

    uint64_t* regPtr() const { return _regPtr; }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    T load(size_t index) const {
        alignas(T) char buffer[sizeof(T)];
        std::memcpy(&buffer, regPtr() + index, sizeof(T));
        return *reinterpret_cast<T*>(&buffer);
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void store(size_t index, T const& t) {
        std::memcpy(regPtr() + index, &t, sizeof(T));
    }

private:
    uint64_t* _regPtr;
};

void const* VoidType(void* sym);

void const* IntType(void* sym, size_t width);

void const* UIntType(void* sym, size_t width);

void const* FloatType(void* sym, size_t width);

void const* RefType(void* sym, void const* base);

void const* MutRefType(void* sym, void const* base);

namespace internal {

size_t constexpr ceildiv(size_t p, size_t q) { return p / q + !!(p % q); }

template <typename FT, size_t Index>
struct RegIndexOf:
    std::integral_constant<
        size_t,
        RegIndexOf<FT, Index - 1>::value +
            ceildiv(sizeof(typename FT::template argument<Index>), 8)> {};

template <typename FT>
struct RegIndexOf<FT, 0>: std::integral_constant<size_t, 0> {};

template <typename T>
void const* makeBaseType(void* sym) {
    if constexpr (std::is_same_v<T, void>) {
        return VoidType(sym);
    }
    if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            return IntType(sym, 8 * sizeof(T));
        }
        else {
            return UIntType(sym, 8 * sizeof(T));
        }
    }
    if constexpr (std::is_floating_point_v<T>) {
        return FloatType(sym, 8 * sizeof(T));
    }
    assert(false);
}

template <typename T>
void const* makeType(void* sym) {
    auto* baseType = makeBaseType<T>(sym);
    if constexpr (std::is_reference_v<T>) {
        using Base = std::remove_reference_t<T>;
        if (std::is_const_v<Base>) {
            return RefType(sym, baseType);
        }
        else {
            return MutRefType(sym, baseType);
        }
    }
    else if constexpr (std::is_pointer_v<T>) {
        using Base = std::remove_pointer_t<T>;
        if (std::is_const_v<Base>) {
            return RefType(sym, baseType);
        }
        else {
            return MutRefType(sym, baseType);
        }
    }
    else {
        return baseType;
    }
}

} // namespace internal

template <typename F>
RuntimeFunction RuntimeFunction::make(std::string name, F& func) {
    using FT = utl::function_traits<F>;
    return [&]<size_t... I>(std::index_sequence<I...>) {
        PtrType ptr =
            [](uint64_t* reg, svm::VirtualMachine* vm, void* userData) {
            F& fn = *static_cast<F*>(userData);
            Loader loader(reg);
            std::tuple args{ loader.load<typename FT::template argument<I>>(
                internal::RegIndexOf<FT, I>::value)... };
            if constexpr (std::is_same<typename FT::result_type, void>::value) {
                std::apply(fn, args);
            }
            else {
                auto result = std::apply(fn, args);
                loader.store(0, result);
            }
        };
        auto sigBuilder = [](void* sym) -> Signature {
            Signature result;
            result.returnType =
                internal::makeType<typename FT::result_type>(sym);
            (result.argTypes.push_back(
                 internal::makeType<typename FT::template argument<I>>(sym)),
             ...);
            return result;
        };
        return RuntimeFunction(std::move(name), ptr, &func, sigBuilder);
    }(std::make_index_sequence<FT::argument_count>());
}

} // namespace scatha

#endif // SCATHA_RUNTIME_FUNCTION_H_
