#include <scatha/Runtime/Program.h>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <utl/scope_guard.hpp>
#include <utl/vector.hpp>

#include "CommonImpl.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;

Program::Program() = default;

Program::Program(std::vector<uint8_t> bin,
                 std::unique_ptr<sema::SymbolTable> sym):
    bin(std::move(bin)), sym(std::move(sym)) {}

Program::Program(Program&&) noexcept = default;

Program& Program::operator=(Program&&) noexcept = default;

Program::~Program() = default;

static utl::small_vector<std::string_view> split(std::string_view str,
                                                 char delim) {
    return str | ranges::views::split(delim) |
           ranges::views::transform([](auto rng) {
               size_t size = utl::narrow_cast<size_t>(ranges::distance(rng));
               return std::string_view(&rng.front(), size);
           }) |
           ranges::to<utl::small_vector<std::string_view>>;
}

std::optional<size_t> Program::findAddress(
    std::string_view name, std::span<QualType const> argTypes) const {
    utl::scope_guard restoreScope = [&] { sym->makeScopeCurrent(nullptr); };
    auto names = split(name, '.');
    for (auto n: names | ranges::views::drop_last(1)) {
        if (!sym->pushScope(n)) {
            return std::nullopt;
        }
    }
    auto* os = sym->lookup<sema::OverloadSet>(names.back());
    if (!os) {
        return std::nullopt;
    }
    auto args = argTypes | ranges::views::transform([&](auto type) {
                    return toSemaType(*sym, type);
                }) |
                ranges::to<utl::small_vector<sema::QualType>>;

    auto itr = ranges::find_if(*os, [&](sema::Function* fn) {
        return ranges::equal(fn->argumentTypes(), args);
    });
    if (itr == os->end()) {
        return std::nullopt;
    }
    return (*itr)->binaryAddress();
}
