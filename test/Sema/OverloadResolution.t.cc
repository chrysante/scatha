#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Common/Allocator.h"
#include "Common/UniquePtr.h"
#include "Sema/Analysis/OverloadResolution.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;

namespace {

struct TestOS {
    static TestOS make(SymbolTable& sym,
                       std::string name,
                       std::initializer_list<std::initializer_list<Type const*>>
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

TEST_CASE("Overload resolution", "[sema]") {
    SymbolTable sym;
    MonotonicBufferAllocator allocator;
    auto makeExpr = [&](QualType type,
                        ValueCategory valueCat) -> ast::Expression* {
        auto result =
            allocate<ast::UnaryExpression>(allocator,
                                           ast::UnaryOperator::Promotion,
                                           ast::UnaryOperatorNotation::Prefix,
                                           nullptr,
                                           SourceRange{});
        result->decorateValue(sym.temporary(type), valueCat);
        return result;
    };
    // clang-format off
    auto f = TestOS::make(sym, "f", {
        /// `f(s64, &[s64])`
        { sym.S64(), sym.reference(QualType::Const(sym.arrayType(sym.S64()))) },
        /// `f(s64, &[s64, 3])`
        { sym.S64(), sym.reference(QualType::Const(sym.arrayType(sym.S64(), 3))) },
    }); // clang-format on

    SECTION("1") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.S64(), LValue),
            makeExpr(sym.arrayType(sym.S64(), 3), LValue)
        }, ORKind::FreeFunction); // clang-format on

        REQUIRE(!result.error);
        CHECK(result.function == f.functions[1].get());
    }

    SECTION("2") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(QualType::Const(sym.S64()), RValue),
            makeExpr(QualType::Const(sym.arrayType(sym.S64())), LValue)
        }, ORKind::FreeFunction); // clang-format on
        REQUIRE(!result.error);
        CHECK(result.function == f.functions[0].get());
    }

    SECTION("3") {
        // clang-format off
        auto result = performOverloadResolution(f.overloadSet.get(), std::array{
            makeExpr(sym.S32(), LValue),
            makeExpr(sym.arrayType(sym.S64(), 4), LValue)
        }, ORKind::FreeFunction); // clang-format on
        REQUIRE(!result.error);
        CHECK(result.function == f.functions[0].get());
    }

    // clang-format off
    auto g = TestOS::make(sym, "g", {
        /// `g(&str)`
        { sym.reference(QualType::Const(sym.Str())) },
    }); // clang-format on

    SECTION("4") {
        // clang-format off
        auto result = performOverloadResolution(g.overloadSet.get(), std::array{
            makeExpr(sym.Str(), LValue)
        }, ORKind::FreeFunction); // clang-format on
        REQUIRE(!result.error);
    }
}
