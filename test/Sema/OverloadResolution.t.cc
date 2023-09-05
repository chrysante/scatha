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
        std::initializer_list<std::initializer_list<QualType>> paramTypeLists) {
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

static UniquePtr<ast::Expression> makeExpr(QualType type) {
    auto result =
        allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                       ast::UnaryOperatorNotation::Prefix,
                                       nullptr,
                                       SourceRange{});
    result->decorateExpr(nullptr, type);
    return result;
}

TEST_CASE("Overload resolution", "[sema]") {
    SymbolTable sym;
    // clang-format off
    auto f = TestOS::make(sym, "f", {
        /// `f(s64, &[s64])`
        { sym.S64(), sym.explRef(QualType::Const(sym.arrayType(sym.S64()))) },
        /// `f(s64, &[s64, 3])`
        { sym.S64(), sym.explRef(QualType::Const(sym.arrayType(sym.S64(), 3))) },
    }); // clang-format on

    SECTION("1") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.implRef(sym.S64())).get(),
            makeExpr(sym.explRef(sym.arrayType(sym.S64(), 3))).get()
        }, false); // clang-format on

        REQUIRE(!result.error);
        CHECK(result.function == f.functions[1].get());
    }

    SECTION("2") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.implRef(QualType::Const(sym.S64()))).get(),
            makeExpr(sym.explRef(QualType::Const(sym.arrayType(sym.S64())))).get()
        }, false); // clang-format on
        REQUIRE(!result.error);
        CHECK(result.function == f.functions[0].get());
    }

    SECTION("3") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.implRef(sym.S32())).get(),
            makeExpr(sym.implRef(sym.arrayType(sym.S64(), 4))).get()
        }, false); // clang-format on
        REQUIRE(result.error);
    }

    // clang-format off
    auto g = TestOS::make(sym, "g", {
        /// `g(&str)`
        { sym.explRef(QualType::Const(sym.Str())) },
    }); // clang-format on

    SECTION("4") {
        // clang-format off
        auto result = performOverloadResolution(g.overloadSet.get(), std::array{
            makeExpr(sym.explRef(sym.Str())).get()
        }, false); // clang-format on
        REQUIRE(!result.error);
    }
}
