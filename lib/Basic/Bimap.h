#ifndef SCATHA_BASIC_BIMAP_H_
#define SCATHA_BASIC_BIMAP_H_

#include <utility>

#include <utl/hashmap.hpp>

#include "Basic/Basic.h"

namespace scatha {

template <typename From, typename To> class Bimap {
    static_assert(!std::is_same_v<std::decay_t<From>, std::decay_t<To>>);

    using FPtr = typename utl::hashmap<From, To>::value_type const *;
    struct ResultBase {
        bool        success() const { return _ptr != nullptr; }
        explicit    operator bool() const { return success(); }

        From const &from() const { return _ptr->first; }
        To const   &to() const { return _ptr->second; }

      private:
        friend class Bimap;
        ResultBase(FPtr ptr): _ptr(ptr) {}

      private:
        FPtr _ptr = nullptr;
    };

  public:
    struct InsertResult: private ResultBase {
        using ResultBase::from;
        using ResultBase::success;
        using ResultBase::to;
        using ResultBase::operator bool;

        using ResultBase::ResultBase;
    };

    struct LookupResult: private ResultBase {
        using ResultBase::from;
        using ResultBase::success;
        using ResultBase::to;
        using ResultBase::operator bool;

        using ResultBase::ResultBase;
    };

  public:
    InsertResult insert(From const &from, To const &to) {
        auto [itr, success]   = _forward.insert({from, to});
        auto [itr2, success2] = _backward.insert({to, from});
        SC_ASSERT(success == success2, "");
        if (!success) {
            return InsertResult(nullptr);
        }
        return InsertResult(&*itr);
    }

    LookupResult lookup(From const &from) const {
        auto itr = _forward.find(from);
        if (itr == _forward.end()) {
            return LookupResult(nullptr);
        }
        return LookupResult(&*itr);
    }

    LookupResult lookup(To const &to) const {
        auto itr = _backward.find(to);
        if (itr == _backward.end()) {
            return LookupResult(nullptr);
        }
        return lookup(itr->second);
    }

    bool contains(From const &from) const { return _forward.contains(from); }
    bool contains(To const &to) const { return _backward.contains(to); }

    auto begin() const { return _forward.begin(); }
    auto end() const { return _forward.end(); }

    bool empty() const { return _forward.empty(); }

  private:
    utl::hashmap<From, To> _forward;
    utl::hashmap<To, From> _backward;
};

} // namespace scatha

#endif // SCATHA_BASIC_BIMAP_H_
