// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ISSUE_VARIANTISSUEBASE_H_
#define SCATHA_ISSUE_VARIANTISSUEBASE_H_

#include <variant>

#include <utl/utility.hpp>

#include <scatha/Basic/Basic.h>
#include <scatha/Issue/ProgramIssue.h>

namespace scatha::issue::internal {

template <typename>
class VariantIssueBase; // Undefined

template <std::derived_from<ProgramIssueBase>... Issues>
class VariantIssueBase<std::variant<Issues...>>: private std::variant<Issues...> {
    static_assert(utl::type_sequence<Issues...>::unique);

private:
    template <typename T>
    static decltype(auto) visitImpl(auto&& f, auto&& v) {
        // clang-format off
        auto const vis = utl::overload{ f, [](issue::internal::ProgramIssuePrivateBase&) -> T {
            if constexpr (!std::is_same_v<T, void>) {
                SC_DEBUGFAIL();
            }
        } };
        // clang-format on
        return std::visit(vis, v);
    }

public:
    using std::variant<Issues...>::variant;

    template <typename T = void>
    decltype(auto) visit(auto&& f) {
        return visitImpl<T>(f, asVariant());
    }

    template <typename T = void>
    decltype(auto) visit(auto&& f) const {
        return visitImpl<T>(f, asVariant());
    }

    template <typename T>
    auto& get() {
        return std::get<T>(asVariant());
    }

    template <typename T>
    auto const& get() const {
        return std::get<T>(asVariant());
    }

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(asVariant());
    }

    Token const& token() const {
        return visit([](issue::ProgramIssueBase const& base) -> auto& { return base.token(); });
    }

    SourceLocation sourceLocation() const { return token().sourceLocation; }

    void setToken(Token token) {
        visit([&](ProgramIssueBase& e) { e.setToken(std::move(token)); });
    }

protected:
    std::variant<Issues...>& asVariant() { return *this; }
    std::variant<Issues...> const& asVariant() const { return *this; }
};

} // namespace scatha::issue::internal

#endif // SCATHA_ISSUE_VARIANTISSUEBASE_H_
