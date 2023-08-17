#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Common/UniquePtr.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

namespace {

struct TestOS {
    static TestOS make(
        SymbolTable& sym,
        std::string name,
        std::initializer_list<std::initializer_list<QualType const*>>
            paramTypeLists) {
        TestOS result;
        result.overloadSet = allocate<OverloadSet>(name, nullptr);
        for (auto types: paramTypeLists) {
            auto f = allocate<Function>(name,
                                        result.overloadSet.get(),
                                        nullptr,
                                        FunctionAttribute::None);
            f->setSignature(FunctionSignature(types, sym.Void()));
            result.overloadSet->add(f.get());
            result.functions.push_back(std::move(f));
        }
        return result;
    }

    UniquePtr<OverloadSet> overloadSet;
    utl::small_vector<UniquePtr<Function>> functions;
};

} // namespace

static UniquePtr<ast::Expression> makeExpr(QualType const* type) {
    auto result =
        allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                       ast::UnaryOperatorNotation::Prefix,
                                       nullptr,
                                       SourceRange{});
    result->decorate(nullptr, type);
    return result;
}

TEST_CASE("Overload resolution", "[sema]") {
    SymbolTable sym;
    // clang-format off
    auto f = TestOS::make(sym, "f", {
        /// `f(s64, &[s64])`
        { sym.S64(), sym.qDynArray(sym.rawS64(), RefConstExpl) },
        /// `f(s64, &[s64, 3])`
        { sym.S64(), sym.qualify(sym.arrayType(sym.rawS64(), 3), RefConstExpl) },
    }); // clang-format on

    SECTION("1") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.qualify(sym.rawS64(), RefMutImpl)).get(),
            makeExpr(sym.qualify(sym.arrayType(sym.rawS64(), 3), RefMutExpl)).get()
        }, false); // clang-format on

        REQUIRE(result);
        CHECK(result.value() == f.functions[1].get());
    }

    SECTION("2") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.qualify(sym.rawS64(), RefConstImpl)).get(),
            makeExpr(sym.qDynArray(sym.rawS64(), RefConstExpl)).get()
        }, false); // clang-format on
        REQUIRE(result);
        CHECK(result.value() == f.functions[0].get());
    }

    SECTION("3") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.qualify(sym.rawS32(), RefMutImpl)).get(),
            makeExpr(sym.qualify(sym.arrayType(sym.rawS64(), 4), RefMutImpl)).get()
        }, false); // clang-format on
        REQUIRE(!result);
    }
}
