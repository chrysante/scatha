#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <type_traits>
#include <typeinfo>
#include <utility>

#include <utl/utility.hpp>

#include "Basic/Basic.h"

namespace scatha::internal {

template <typename T>
struct DyncastTypeToEnumImpl;

template <auto EnumValue>
struct DyncastEnumToTypeImpl;

template <typename T>
using DecayAll = std::remove_const_t<std::remove_pointer_t<std::decay_t<T>>>;

template <typename T>
decltype(DyncastTypeToEnumImpl<DecayAll<T>>::value, std::true_type{}) isDynamicImpl(int);

template <typename T>
std::false_type isDynamicImpl(...);

template <typename T>
concept Dynamic = decltype(isDynamicImpl<T>(0))::value;

template <typename To, typename From>
concept DynCheckable = Dynamic<From> && Dynamic<To>; // TODO: Check that To and From a part of the same hierarchy.

template <typename To, typename From>
concept DynCastable = DynCheckable<To, From> && requires(From from) {
    static_cast<To>(from);
};

template <typename T>
using DCEnumType = decltype(DyncastTypeToEnumImpl<std::remove_cvref_t<T>>::value);

} // namespace scatha::internal

//---==========================================================================
//---=== Public interface =====================================================
//---==========================================================================

/// Mandatory customization point for the \p dyncast facilities. Every object in the inheritance hierarchy must be
/// uniquely mapped to an enum or integral value. Using an enum is recommended. Use this macro at file scope to
/// identify types in the hierarchy with a unique integral value.
#define SC_DYNCAST_MAP(type, enumValue)                                                                                \
    template <>                                                                                                        \
    struct ::scatha::internal::DyncastTypeToEnumImpl<type>: std::integral_constant<decltype(enumValue), enumValue> {}; \
    template <>                                                                                                        \
    struct ::scatha::internal::DyncastEnumToTypeImpl<enumValue>: std::type_identity<type> {}

namespace scatha {

// clang-format off

/// Simple customization point for the \p dyncast facilities. If the types in the inheritance hierarchy expose their
/// runtime type identifiers by a \p .type() method, this customiziation point is not needed. Otherwise define a
/// function \p dyncastGetType() with compatible signature for every type in the same namespace as the type. This can
/// of course be a (constrained) template.
template <typename T>
auto dyncastGetType(T const& t)
requires requires { { t.type() } -> std::convertible_to<internal::DCEnumType<T>>; }
{
    return t.type();
}

// clang-format on

/// Optional second customization point. All fields below must be implemented. If a custom implementation for
/// \p type(...) is given, the first customization point \p dyncastGetType() is not used.
template <typename Enum>
struct DyncastTraits {
    static Enum type(auto const& t) { return dyncastGetType(t); }
    static constexpr Enum first = Enum{ 0 };
    static constexpr Enum last  = Enum::_count;
};

/// Visit the object \p obj as its most derived type.
/// \param obj An object of type \p T which has support for the \p dyncast facilities.
/// \param fn A callable which can be invoked with any of \p T 's derived types. The possible return types of \p fn
///        when invoked with \p T 's derived types must have a common type determined by \p std::common_type.
/// \returns \p std::invoke(fn,static_cast<MOST_DERIVED_TYPE>(obj)) where \p MOST_DERIVED_TYPE is the most derived type
///          of \p obj with the same cv-ref qualifications as \p obj.
template <typename T, typename F>
requires internal::Dynamic<T>
decltype(auto) visit(T&& obj, F&& fn);

/// Check if \p from 's dymamic type is \p To or derived from \p To.
template <typename To, typename From>
requires scatha::internal::DynCheckable<To, From>
bool isa(From* from);

/// Check if \p from 's dymamic type is \p To or derived from \p To.
template <typename To, typename From>
requires scatha::internal::DynCheckable<To, From>
bool isa(From& from);

/// Downwards cast of \p from in its class hierarchy.
/// \param from Pointer to an object of type \p From. Pointer must not be null.
/// \returns A pointer of derived type \p To or null if \p *from is not of type \p To.
template <typename To, typename From>
requires internal::DynCastable<To, From*> && std::is_pointer_v<To>
constexpr To dyncast(From* from);

/// Downwards cast of \p from in its class hierarchy.
/// \param from Reference to an object of type \p From.
/// \returns A Reference to the object of derived type \p T.o
/// \throws \p std::bad_cast if \p from is not of type \p To.
template <typename To, typename From>
requires internal::DynCastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To dyncast(From& from);

/// Downwards cast of \p from in its class hierarchy.
/// \param from Pointer to an object of type \p From. Pointer must not be null.
/// \returns A pointer of derived type \p To.
/// \warning Traps if \p *from is not of type \p To.
template <typename To, typename From>
requires internal::DynCastable<To, From*> && std::is_pointer_v<To>
constexpr To cast(From* from);

/// Downwards cast of \p from in its class hierarchy.
/// \param from Reference to an object of type \p From .
/// \returns A reference of derived type \p To .
/// \warning Traps if \p from is not of type \p To .
template <typename To, typename From>
requires internal::DynCastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To cast(From& from);

} // namespace scatha

//---==========================================================================
//---=== Implementation =======================================================
//---==========================================================================

namespace scatha::internal {

template <typename T>
inline constexpr auto DyncastTypeToEnum = DyncastTypeToEnumImpl<T>::value;

template <auto EnumValue>
using DyncastEnumToType = typename DyncastEnumToTypeImpl<EnumValue>::type;

// clang-format off
template <typename Enum>
struct DyncastTraitsImpl: DyncastTraits<Enum> {
public:
    using DyncastTraits<Enum>::type;
    using DyncastTraits<Enum>::first;
    using DyncastTraits<Enum>::last;
    static constexpr size_t numElements = static_cast<size_t>(last) - static_cast<size_t>(first);

    template <Enum TestType>
    static bool is(auto const& t) {
        return ISADispatchArray[static_cast<size_t>(TestType)][static_cast<size_t>(type(t))];
    }

private:
    template <Enum CurrentTestType, Enum TargetTestType, typename ActualType>
    requires std::is_convertible_v<ActualType, DyncastEnumToType<CurrentTestType>>
    static constexpr bool isExactly() { return CurrentTestType == TargetTestType; }

    template <Enum CurrentTestType, Enum TargetTestType, typename ActualType>
    static constexpr bool isExactly() {
        return false;
    }

    template <Enum TargetTestType, typename ActualType, size_t... I>
    static constexpr bool walkTree(std::index_sequence<I...>) {
        constexpr bool result = (... || isExactly<Enum{ I }, TargetTestType, ActualType>());
        return result;
    }

    template <Enum TargetTestType, typename ActualType>
    static constexpr bool isImpl() {
        return walkTree<TargetTestType, ActualType>(std::make_index_sequence<numElements>{});
    }

    /// Build a 2D boolean matrix at compile time indicating if a dynamic cast is possible for every origin-destination
    /// pair.
    static constexpr auto makeISADispatchArray() {
        return []<size_t... I>(std::index_sequence<I...>) {
            return std::array{
                []<Enum TestType, size_t... J>(std::index_sequence<J...>) {
                    return std::array{ isImpl<TestType, DyncastEnumToType<Enum{ J }>>()... };
                }.template operator()<Enum{ I }>(std::make_index_sequence<numElements>{})...
            };
        }(std::make_index_sequence<numElements>{});
    }
    static constexpr auto ISADispatchArray = makeISADispatchArray();
};
// clang-format on

/// This machinery is needed to make visiting subtrees of the entire inheritance hierarchy possible. Without it,
/// \p std::invoke_result would fail on code paths that are never executed.

/// Tag type to indicate that a function is not invocable for given parameters.
enum class NotInvocable;

/// Wrapper that evaluates to \p NotInvocable if \p F is not invocable with \p Args...
template <typename F, typename... Args>
using InvokeResult = typename std::conditional_t<std::is_invocable_v<F, Args...>,
                                                 std::invoke_result<F, Args...>,
                                                 std::type_identity<NotInvocable>>::type;

/// Wrapper for \p std::common_type to ignore \p NotInvocable .
template <typename...>
struct CommonTypeWrapper;

template <typename A, typename B, typename... Rest>
struct CommonTypeWrapper<A, B, Rest...> {
    using type = typename CommonTypeWrapper<std::common_type_t<A, B>, Rest...>::type;
};

template <typename... Rest, typename B>
struct CommonTypeWrapper<NotInvocable, B, Rest...> {
    using type = typename CommonTypeWrapper<B, Rest...>::type;
};

template <typename A, typename... Rest>
struct CommonTypeWrapper<A, NotInvocable, Rest...> {
    using type = typename CommonTypeWrapper<A, Rest...>::type;
};

template <typename... Rest>
struct CommonTypeWrapper<NotInvocable, NotInvocable, Rest...> {
    using type = typename CommonTypeWrapper<Rest...>::type;
};

template <typename A>
struct CommonTypeWrapper<A> {
    using type = A;
};

template <typename Enum,
          typename GivenType,
          typename F,
          typename I = std::make_index_sequence<DyncastTraitsImpl<Enum>::numElements>>
struct DispatchReturnType;

template <typename Enum, typename GivenType, typename F, size_t... I>
struct DispatchReturnType<Enum, GivenType, F, std::index_sequence<I...>> {
    using type = typename CommonTypeWrapper<
        InvokeResult<F, utl::copy_cvref_t<GivenType, DyncastEnumToType<Enum{ I }>>>...>::type;
};

} // namespace scatha::internal

// clang-format off

template <typename T, typename F>
requires scatha::internal::Dynamic<T>
decltype(auto) scatha::visit(T&& obj, F&& fn) {
    using namespace internal;
    using EnumType = DCEnumType<T>;
    using Traits   = DyncastTraitsImpl<EnumType>;
    static_assert(static_cast<size_t>(Traits::first) == 0, "For now, this simplifies the implementation.");
    static constexpr size_t numElems = Traits::numElements;
    using ReturnType                 = typename DispatchReturnType<EnumType, T&&, F&&>::type;
    using DispatchPtrType            = ReturnType(*)(T&&, F&&);
    static constexpr std::array<DispatchPtrType, numElems> dispatchPtrs = [&]<size_t... I>(std::index_sequence<I...>) {
        return std::array<DispatchPtrType, numElems>{ [](T&& t, F&& f) -> ReturnType {
            using TargetType                  = utl::copy_cvref_t<T&&, DyncastEnumToType<EnumType{ I }>>;
            constexpr bool staticallyCastable = requires { static_cast<TargetType>(t); };
            static_assert(std::is_reference_v<TargetType>, "To avoid copies when performing static_cast.");
            if constexpr (!staticallyCastable) {
                /// If we can't even \p static_cast there is no way this can be invoked.
                /// We need to use \p I in this path also, otherwise we can't fold over this expression later. This
                /// might be a bug in clang.
                (void)I;
                SC_UNREACHABLE();
            }
            else if constexpr (std::is_convertible_v<T&&, TargetType> && !std::is_same_v<T&&, TargetType>) {
                /// If we can cast implicitly but destination type is not the same, this means we go up the hierarchy.
                /// Since we are dispatching on the most derived type, this path should be unreachable.
                (void)I;
                SC_UNREACHABLE();
            }
            else {
                return std::invoke(std::forward<F>(f), static_cast<TargetType>(t));
            }
        }... };
    }(std::make_index_sequence<numElems>{});
    size_t const index = static_cast<size_t>(DyncastTraits<EnumType>::type(obj));
    auto const dispatchFn = dispatchPtrs[index];
    return dispatchFn(std::forward<T>(obj), std::forward<F>(fn));
}

// clang-format on

template <typename To, typename From>
requires scatha::internal::DynCheckable<To, From>
bool scatha::isa(From* from) {
    using namespace internal;
    using EnumType   = decltype(DyncastTypeToEnum<std::remove_const_t<From>>);
    using ToStripped = std::remove_const_t<std::remove_pointer_t<To>>;
    return DyncastTraitsImpl<EnumType>::template is<DyncastTypeToEnum<ToStripped>>(*from);
}

template <typename To, typename From>
requires scatha::internal::DynCheckable<To, From>
bool scatha::isa(From& from) {
    return isa<To>(&from);
}

template <typename To, typename From>
requires scatha::internal::DynCastable<To, From*> && std::is_pointer_v<To>
constexpr To scatha::dyncast(From* from) {
    if (isa<internal::DecayAll<To>>(from)) {
        return static_cast<To>(from);
    }
    return nullptr;
}

template <typename To, typename From>
requires scatha::internal::DynCastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To scatha::dyncast(From& from) {
    using ToNoRef = std::remove_reference_t<To>;
    if (auto* result = dyncast<ToNoRef*>(&from)) {
        return *result;
    }
    throw std::bad_cast();
}

template <typename To, typename From>
requires scatha::internal::DynCastable<To, From*> && std::is_pointer_v<To>
constexpr To scatha::cast(From* from) {
    SC_ASSERT(dyncast<To>(from) != nullptr, "Cast failed.");
    return static_cast<To>(from);
}

template <typename To, typename From>
requires scatha::internal::DynCastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To scatha::cast(From& from) {
    using ToNoRef = std::remove_reference_t<To>;
    return *cast<ToNoRef*>(&from);
}

#endif // SCATHA_COMMON_DYNCAST_H_
