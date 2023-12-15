#ifndef SCATHA_RUNTIME_LIBSUPPORT_H_
#define SCATHA_RUNTIME_LIBSUPPORT_H_

#include <functional>
#include <typeindex>
#include <vector>

#include <scatha/Runtime/Support.h>

#define SC_EXPORT_FUNCTION(FUNCTION, NAME)                                     \
    SC_STATIC_CONSTRUCTOR {                                                    \
        ::scatha::internal::globalLibDecls.push_back([]() {                    \
            return ::std::pair{                                                \
                (char const*)NAME,                                             \
                ::scatha::internal::extractSignature<decltype(FUNCTION)>()     \
            };                                                                 \
        });                                                                    \
        ::scatha::internal::globalLibDefines.push_back([] {                    \
            return ::std::pair{                                                \
                (char const*)NAME,                                             \
                ::scatha::internal::makeImplAndUserPtr(FUNCTION).first         \
            };                                                                 \
        });                                                                    \
    };

namespace scatha {

struct CppSignature {
    std::type_index ret;
    std::vector<std::type_index> params;
};

///
using DeclareCallback = void (*)(void* context,
                                 char const* name,
                                 CppSignature sig);

///
using DefineCallback = void (*)(void* context,
                                size_t index,
                                char const* name,
                                InternalFuncPtr impl,
                                void* userptr);

} // namespace scatha

namespace scatha::internal {

using DeclPair = std::pair<char const*, CppSignature>;

using DefPair = std::pair<char const*, InternalFuncPtr>;

template <typename>
struct ExtractCppSig;

template <typename R, typename... Args>
struct ExtractCppSig<std::function<R(Args...)>> {
    static CppSignature Impl() {
        return CppSignature{ typeid(R), { typeid(Args)... } };
    };
};

template <typename F>
CppSignature extractSignature() {
    return internal::ExtractCppSig<decltype(std::function{
        std::declval<F>() })>::Impl();
}

extern std::vector<std::function<DeclPair()>> globalLibDecls;

extern std::vector<std::function<DefPair()>> globalLibDefines;

} // namespace scatha::internal

extern "C" void internal_declareFunctions(scatha::DeclareCallback callback,
                                          void* context);

extern "C" void internal_defineFunctions(scatha::DefineCallback callback,
                                         void* context);

#endif // SCATHA_RUNTIME_LIBSUPPORT_H_
