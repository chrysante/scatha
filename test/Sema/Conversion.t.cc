#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Implicit conversion rank", "[sema]") {
    sema::SymbolTable sym;
    ast::UnaryExpression expr(ast::UnaryOperator::Promotion,
                              nullptr,
                              SourceRange{});
    SECTION("1") {
        expr.decorate(nullptr, sym.U16());
        CHECK(implicitConversionRank(&expr, sym.U16()).value() == 0);
    }
    SECTION("2") {
        expr.decorate(nullptr, sym.S64());
        CHECK(implicitConversionRank(&expr, sym.S64()).value() == 0);
    }
    SECTION("3") {
        expr.decorate(nullptr, sym.U16());
        CHECK(implicitConversionRank(&expr, sym.S32()).value() == 1);
    }
    SECTION("4") {
        expr.decorate(nullptr, sym.U16());
        CHECK(implicitConversionRank(&expr, sym.U32()).value() == 1);
    }
    SECTION("5") {
        expr.decorate(nullptr, sym.S16());
        CHECK(!implicitConversionRank(&expr, sym.U32()));
    }
}

TEST_CASE("Arithemetic conversions", "[sema]") {
    sema::SymbolTable sym;
    IssueHandler iss;
    ast::UnaryExpression
        base(ast::UnaryOperator::Promotion,
             allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                            nullptr,
                                            SourceRange{}),
             SourceRange{});
    auto* expr   = base.operand();
    auto setType = [&](QualType const* type) { expr->decorate(nullptr, type); };
    auto set     = [&](QualType const* type, auto value) {
        setType(type);
        auto* arithType = cast<ArithmeticType const*>(type->base());
        APInt apValue(value, arithType->bitwidth());
        expr->setConstantValue(
            allocate<IntValue>(apValue, arithType->isSigned()));
    };
    auto getResult = [&] {
        return cast<IntValue const*>(
                   cast<ast::Expression const*>(expr->parent())
                       ->constantValue())
            ->value();
    };

    /// # Widening
    SECTION("u32(5) to u64") {
        set(sym.U32(), 5);
        CHECK(convertImplicitly(expr, sym.U64(), iss));
        CHECK(iss.empty());
    }
    SECTION("u64(5) to u64") {
        set(sym.U64(), 5);
        CHECK(convertImplicitly(expr, sym.U64(), iss));
        CHECK(iss.empty());
    }

    /// # Explicit widening
    SECTION("byte(5) to s64") {
        set(sym.Byte(), 5);
        CHECK(convertExplicitly(expr, sym.S64(), iss));
        CHECK(iss.empty());
    }

    /// # Narrowing
    SECTION("s64(5) to s8") {
        set(sym.S64(), 5);
        CHECK(convertImplicitly(expr, sym.S8(), iss));
        CHECK(iss.empty());
    }
    SECTION("s64(<unknown>) to s8") {
        setType(sym.S64());
        CHECK(!convertImplicitly(expr, sym.S8(), iss));
        CHECK(convertExplicitly(expr, sym.S8(), iss));
    }
    SECTION("S64(-1) to u32") {
        set(sym.S64(), -1);
        CHECK(!convertImplicitly(expr, sym.U32(), iss));
        CHECK(convertExplicitly(expr, sym.U32(), iss));
        APInt result = getResult();
        CHECK(result.bitwidth() == 32);
        CHECK(ucmp(result, ~0u) == 0);
    }
    SECTION("U32(0x1000000F) to s16") {
        set(sym.U32(), 0x1000000F);
        CHECK(!convertImplicitly(expr, sym.S16(), iss));
        CHECK(convertExplicitly(expr, sym.S16(), iss));
        APInt result = getResult();
        CHECK(result.bitwidth() == 16);
        CHECK(ucmp(result, 0xF) == 0);
    }
    SECTION("-1 to u64") {
        set(sym.S64(), -1);
        CHECK(!convertImplicitly(expr, sym.U64(), iss));
        CHECK(convertExplicitly(expr, sym.U32(), iss));
    }
    SECTION("s64(5) to byte") {
        set(sym.S64(), 5);
        CHECK(convertImplicitly(expr, sym.Byte(), iss));
        CHECK(iss.empty());
    }
    SECTION("s64(256) to byte") {
        set(sym.S64(), 256);
        CHECK(!convertImplicitly(expr, sym.Byte(), iss));
    }
}

TEST_CASE("Common type", "[sema]") {
    SymbolTable sym;
    SECTION("s64, s64 -> s64")
    CHECK(commonType(sym, sym.S64(), sym.S64()) == sym.S64());
    SECTION("'mut s64, 'mut s64 -> 'mut s64")
    CHECK(commonType(sym, sym.S64(RefMutImpl), sym.S64(RefMutImpl)) ==
          sym.S64(RefMutImpl));
    SECTION("'s64, 'mut s64 -> 's64")
    CHECK(commonType(sym, sym.S64(RefConstImpl), sym.S64(RefMutImpl)) ==
          sym.S64(RefConstImpl));
    SECTION("'s64, s64 -> s64")
    CHECK(commonType(sym, sym.S64(RefConstImpl), sym.S64()) == sym.S64());
    SECTION("'s64, 'mut s32 -> s64")
    CHECK(commonType(sym, sym.S64(RefConstImpl), sym.S32(RefMutImpl)) ==
          sym.S64());
    SECTION("'s64, s32 -> s64")
    CHECK(commonType(sym, sym.S64(RefConstImpl), sym.S32()) == sym.S64());
    SECTION("&mut s64, 'mut s64 -> None")
    CHECK(!commonType(sym, sym.S64(RefMutExpl), sym.S64(RefMutImpl)));
    SECTION("s64, byte -> None")
    CHECK(!commonType(sym, sym.S64(), sym.Byte()));
    SECTION("s64, u64 -> None")
    CHECK(!commonType(sym, sym.S64(), sym.U64()));
    SECTION("s64, u32 -> s64")
    CHECK(commonType(sym, sym.S64(), sym.U32()) == sym.S64());
}
