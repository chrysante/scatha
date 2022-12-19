#ifndef SCATHA_COMMON_DYNCAST_H_
#define SCATHA_COMMON_DYNCAST_H_

#include <type_traits>
#include <utility>
#include <typeinfo>

#include <utl/utility.hpp>

#include "Basic/Basic.h"

/// Customization point for the \p dyncast facilities. Every object in the inheritance hierarchy must correspond to
/// an integral (or \p enum ) value. Use this macro at file scope to identify types in the hierarchy with a unique
/// integral value.
#define SC_DYNCAST_REGISTER_PAIR(type, enum)                                                             \
template <>                                                                                              \
struct ::scatha::internal::DyncastTypeToEnumImpl<type>: std::integral_constant<decltype(enum), enum> {}; \
template <>                                                                                              \
struct ::scatha::internal::DyncastEnumToTypeImpl<enum>: std::type_identity<type> {}

namespace scatha {

namespace internal {

template <typename T>
struct DyncastTypeToEnumImpl;

template <auto EnumValue>
struct DyncastEnumToTypeImpl;

template <typename T>
using DecayAll = std::remove_const_t<std::remove_pointer_t<std::decay_t<T>>>;

template <typename T>
inline constexpr bool isDynEnabled = DyncastTypeToEnumImpl<T>::value == DyncastTypeToEnumImpl<T>::value;

template <typename To, typename From>
concept Dyncastable =
    isDynEnabled<DecayAll<From>> && isDynEnabled<DecayAll<To>> &&
    requires(From from) { static_cast<To>(from); };

} // namespace internal

template <typename T>
inline constexpr auto DyncastTypeToEnum = internal::DyncastTypeToEnumImpl<T>::value;

template <auto EnumValue>
using DyncastEnumToType = typename internal::DyncastEnumToTypeImpl<EnumValue>::type;

/// Simple customization point for the \p dyncast facilities. If the types in the inheritance hierarchy expose their
/// runtime type identifiers by a \p .type() method, this customiziation point need not be used.
/// Otherwise define a function \p dyncastGetType with compatible signature for every type in the same namespace as the
/// type.
auto dyncastGetType(auto const& t) requires requires { t.type(); } { return t.type(); }

template <typename Enum>
struct DyncastTraitsBase {
    static Enum type(auto const& t) { return dyncastGetType(t); }
    static constexpr Enum first = Enum{ 0 };
    static constexpr Enum last = Enum::_count;
};

template <typename Enum>
struct DyncastTraits: DyncastTraitsBase<Enum> {
public:
    using DyncastTraitsBase<Enum>::type;
    using DyncastTraitsBase<Enum>::first;
    using DyncastTraitsBase<Enum>::last;
    static constexpr size_t numElements = static_cast<size_t>(last) - static_cast<size_t>(first);

    template <Enum TestType>
    static bool is(auto const& t) {
        return ISADispatchArray[static_cast<size_t>(TestType)][static_cast<size_t>(type(t))];
    }
    
private:
    template <Enum CurrentTestType, Enum TargetTestType, typename ActualType>
    requires std::is_convertible_v<ActualType, DyncastEnumToType<CurrentTestType>>
    static constexpr bool isExactly() {
        return CurrentTestType == TargetTestType;
    }
    
    template <Enum CurrentTestType, Enum TargetTestType, typename ActualType>
    static constexpr bool isExactly() { return false; }
    
    template <Enum TargetTestType, typename ActualType, size_t... I>
    static constexpr bool walkTree(std::index_sequence<I...>) {
        constexpr bool result = (... || isExactly<Enum{ I }, TargetTestType, ActualType>());
        return result;
    }
    
    template <Enum TargetTestType, typename ActualType>
    static constexpr bool isImpl() {
        return walkTree<TargetTestType, ActualType>(std::make_index_sequence<numElements>{});
    }
    
    /// Build a 2D boolean matrix at compile time indicating if a dynamic cast is possible for every origin-destination pair.
    static constexpr auto makeISADispatchArray() {
        return []<size_t... I>(std::index_sequence<I...>) {
            return std::array{
                []<Enum TestType, size_t... J>(std::index_sequence<J...>) {
                    return std::array{
                        isImpl<TestType, DyncastEnumToType<Enum{ J }>>()...
                    };
                }.template operator()<Enum{ I }>(std::make_index_sequence<numElements>{})...
            };
        }(std::make_index_sequence<numElements>{});
    }
    
    static constexpr auto ISADispatchArray = makeISADispatchArray();
};

namespace internal {

template <typename Enum, typename GivenType, typename F, typename I>
struct DispatchReturnType;

template <typename Enum, typename GivenType, typename F, size_t... I>
struct DispatchReturnType<Enum, GivenType, F, std::index_sequence<I...>> {
    using type = std::common_type_t<std::invoke_result_t<F, utl::copy_cvref_t<GivenType, DyncastEnumToType<Enum{ I }>>>...>;
};

} // namespace internal

/// Visit the object \p obj as its most derived type.
/// \param obj An object of type \p T which has support for the \p dyncast facilities.
/// \param fn A callable which can be invoked with any of \p T 's derived types. The possible return types of \p fn
///        must a a common type determined by \p std::common_type .
/// \returns \p std::invoke(fn,static_cast<MOST_DERIVED_TYPE>(obj)) where \p MOST_DERIVED_TYPE is the most derived type
///          of \p obj with the same cv-ref qualifications as \p obj .
template <typename T, typename F>
requires internal::isDynEnabled<std::remove_cvref_t<T>>
decltype(auto) visit(T&& obj, F&& fn) {
    using EnumType = decltype(DyncastTypeToEnum<std::remove_cvref_t<T>>);
    using Traits = DyncastTraits<EnumType>;
    static_assert(static_cast<size_t>(Traits::first) == 0, "For now, this simplifies the implementation.");
    static constexpr size_t numElems = Traits::numElements;
    using ReturnType = typename internal::DispatchReturnType<EnumType, T&&, F&&, std::make_index_sequence<numElems>>::type;
    using DispatchPtrType = ReturnType(*)(T&&, F&&);
    static constexpr std::array<DispatchPtrType, numElems> dispatchPtrs = [&]<size_t... I>(std::index_sequence<I...>) {
        return std::array<DispatchPtrType, numElems>{
            [](T&& t, F&& f) -> ReturnType {
                using TargetType = utl::copy_cvref_t<T&&, DyncastEnumToType<EnumType{ I }>>;
                if constexpr (requires{ static_cast<TargetType>(t); }) {
                    return std::invoke(std::forward<F>(f), static_cast<TargetType>(t));
                }
                else {
                    /// We need to use \p I in this path also, otherwise we can't fold over this expression later. This might be a bug in clang.
                    (void)I;
                    SC_UNREACHABLE();
                }
            }...
        };
    }(std::make_index_sequence<numElems>{});
    return dispatchPtrs[static_cast<size_t>(DyncastTraits<EnumType>::type(obj))](std::forward<T>(obj), std::forward<F>(fn));
}

namespace internal {

template <typename To, typename From>
constexpr To dyncastImpl(From const* from) {
    SC_EXPECT(from != nullptr, "'from' must not be null.");
    using EnumType = decltype(DyncastTypeToEnum<From>);
    using ToStripped = std::remove_const_t<std::remove_pointer_t<To>>;
    if (DyncastTraits<EnumType>::template is<DyncastTypeToEnum<ToStripped>>(*from)) {
        return static_cast<To>(from);
    }
    return nullptr;
}

} // namespace internal

/// Downwards cast of \p from in its class hierarchy.
/// \param from Pointer to an object of type \p From . Pointer must not be null.
/// \returns A pointer of derived type \p To or null if \p *from is not of type \p To .
template <typename To, typename From> requires internal::Dyncastable<To, From*> && std::is_pointer_v<To>
constexpr To dyncast(From* from) {
    using ToStripped = std::remove_const_t<std::remove_pointer_t<To>>;
    return const_cast<To>(internal::dyncastImpl<ToStripped const*>(from));
}

/// Downwards cast of \p from in its class hierarchy.
/// \param from Reference to an object of type \p From .
/// \returns A Reference to the object of derived type \p To .
/// \throws \p std::bad_cast if \p from is not of type \p To .
template <typename To, typename From> requires internal::Dyncastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To dyncast(From& from) {
    using ToNoRef = std::remove_reference_t<To>;
    if (auto* result = dyncast<ToNoRef*>(&from)) {
        return *result;
    }
    throw std::bad_cast();
}

/// Downwards cast of \p from in its class hierarchy.
/// \param from Pointer to an object of type \p From . Pointer must not be null.
/// \returns A pointer of derived type \p To .
/// \warning Traps if \p *from is not of type \p To .
template <typename To, typename From> requires internal::Dyncastable<To, From*> && std::is_pointer_v<To>
constexpr To cast(From* from) {
    SC_ASSERT(dyncast<To>(from) != nullptr, "Cast failed.");
    return static_cast<To>(from);
}

/// Downwards cast of \p from in its class hierarchy.
/// \param from Reference to an object of type \p From .
/// \returns A reference of derived type \p To .
/// \warning Traps if \p from is not of type \p To .
template <typename To, typename From> requires internal::Dyncastable<To, From&> && std::is_lvalue_reference_v<To>
constexpr To cast(From& from) {
    using ToNoRef = std::remove_reference_t<To>;
    return *cast<ToNoRef*>(&from);
}

} // namespace scatha

#endif // SCATHA_COMMON_DYNCAST_H_

