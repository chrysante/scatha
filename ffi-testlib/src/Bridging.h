#ifndef SCATHA_BRIDGING_H_
#define SCATHA_BRIDGING_H_

#define SC_BRIDGING_CONCAT(a, b)      SC_BRIDGING_CONCAT_IMPL(a, b)
#define SC_BRIDGING_CONCAT_IMPL(a, b) a##b

#define SC_EXPORT(FUNCTION, NAME)                                              \
    extern "C" void SC_BRIDGING_CONCAT(sc_ffi_,                                \
                                       NAME)(void* reg, void*, void*) {        \
        scatha::bridging::Invoke<decltype(FUNCTION)>::impl(static_cast<char*>( \
                                                               reg),           \
                                                           FUNCTION);          \
    }

namespace scatha {
namespace bridging {

using size_t = unsigned long long;

template <typename T>
struct RemoveRef {
    using type = T;
};

template <typename T>
struct RemoveRef<T&> {
    using type = T;
};

template <typename T>
struct RemoveRef<T&&> {
    using type = T;
};

template <size_t N, typename T, typename... U>
struct Nth: Nth<N - 1, U...> {};

template <typename T, typename... U>
struct Nth<0, T, U...> {
    using type = T;
};

template <typename... Args>
struct TypeList {
    static constexpr size_t size = sizeof...(Args);
};

template <size_t...>
struct IndexList {};

template <size_t Size, size_t... Indices>
struct MakeIndexList: MakeIndexList<Size - 1, Size - 1, Indices...> {};

template <size_t... Indices>
struct MakeIndexList<0, Indices...> {
    using type = IndexList<Indices...>;
};

template <typename Types, typename Indices>
struct TupleBase;

template <typename T, size_t Index>
struct TupleLeaf {
    T value;
};

template <typename... T, size_t... Indices>
struct TupleBase<TypeList<T...>, IndexList<Indices...>>:
    TupleLeaf<T, Indices>... {};

template <typename... T>
struct Tuple:
    TupleBase<TypeList<T...>, typename MakeIndexList<sizeof...(T)>::type> {
    static const size_t size = sizeof...(T);
};

template <size_t Index, typename... T>
typename Nth<Index, T...>::type& get(Tuple<T...>& tuple) {
    using Type = typename Nth<Index, T...>::type;
    return static_cast<TupleLeaf<Type, Index>&>(tuple).value;
}

inline size_t roundup(size_t n) {
    return (n + 7) & static_cast<unsigned long long>(-8ll);
}

template <typename T>
T load(char*& reg) {
    T res = *reinterpret_cast<T*>(reg);
    reg += roundup(sizeof(T));
    return res;
}

template <size_t Index, typename Tuple>
void accTupleOne(Tuple& tuple, char*& reg) {
    using T = typename RemoveRef<decltype(get<Index>(tuple))>::type;
    get<Index>(tuple) = bridging::load<T>(reg);
}

template <size_t Index, size_t Last>
struct AccTuple {
    template <typename Tuple>
    static void impl(Tuple& tuple, char*& reg) {
        accTupleOne<Index>(tuple, reg);
        AccTuple<Index + 1, Last>::impl(tuple, reg);
    }
};

template <size_t Index>
struct AccTuple<Index, Index> {
    template <typename Tuple>
    static void impl(Tuple& tuple, char*& reg) {
        accTupleOne<Index>(tuple, reg);
    }
};

template <typename R, typename... Args, size_t... Indices>
static R loadAndInvoke(IndexList<Indices...>, char* reg, R (*ptr)(Args...)) {
    Tuple<Args...> args;
    AccTuple<0, sizeof...(Args) - 1>::impl(args, reg);
    return ptr(get<Indices>(args)...);
}

template <typename R, typename... Args>
struct InvokeReturn {
    static R impl(char* reg, R (*ptr)(Args...)) {
        return loadAndInvoke<R, Args...>(typename MakeIndexList<sizeof...(
                                             Args)>::type{},
                                         reg,
                                         ptr);
    }
};

template <typename R>
struct InvokeReturn<R> {
    static R impl(char* reg, R (*ptr)()) { return ptr(); }
};

template <typename R, typename... Args>
struct InvokeStore {
    static void impl(char* reg, R (*ptr)(Args...)) {
        auto res = InvokeReturn<R, Args...>::impl(reg, ptr);
        *reinterpret_cast<R*>(reg) = res;
    }
};

template <typename... Args>
struct InvokeStore<void, Args...> {
    static void impl(char* reg, void (*ptr)(Args...)) {
        InvokeReturn<void, Args...>::impl(reg, ptr);
    }
};

template <typename>
struct Invoke;

template <typename R, typename... Args>
struct Invoke<R(Args...)> {
    static void impl(char* reg, R (*ptr)(Args...)) {
        InvokeStore<R, Args...>::impl(reg, ptr);
    }
};

} // namespace bridging
} // namespace scatha

#endif // SCATHA_BRIDGING_H_
