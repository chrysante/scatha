#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Common/Allocator.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/DtorStack.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;
using enum ValueCategory;
using enum ConversionKind;

TEST_CASE("Implicit conversion rank", "[sema]") {
    sema::SymbolTable sym;
    ast::UnaryExpression expr(ast::UnaryOperator::Promotion,
                              ast::UnaryOperatorNotation::Prefix,
                              nullptr,
                              SourceRange{});
    auto fromCat = GENERATE(LValue, RValue);
    auto toCat = GENERATE(LValue, RValue);
    if (fromCat == RValue && toCat == LValue) {
        return;
    }
    INFO("From: " << fromCat);
    INFO("To:   " << toCat);
    SECTION("1") {
        expr.decorateValue(sym.temporary(sym.U16()), fromCat);
        auto conv = computeConversion(ConversionKind::Implicit,
                                      &expr,
                                      sym.U16(),
                                      toCat);
        CHECK(computeRank(conv.value()) == 0);
    }
    SECTION("2") {
        expr.decorateValue(sym.temporary(sym.S64()), fromCat);
        auto conv = computeConversion(ConversionKind::Implicit,
                                      &expr,
                                      sym.S64(),
                                      toCat);
        CHECK(computeRank(conv.value()) == 0);
    }
    SECTION("3") {
        expr.decorateValue(sym.temporary(sym.U16()), fromCat);
        auto conv = computeConversion(ConversionKind::Implicit,
                                      &expr,
                                      sym.S32(),
                                      toCat);
        CHECK(computeRank(conv.value()) == 1);
    }
    SECTION("4") {
        expr.decorateValue(sym.temporary(sym.U16()), fromCat);
        auto conv = computeConversion(ConversionKind::Implicit,
                                      &expr,
                                      sym.U32(),
                                      toCat);
        CHECK(computeRank(conv.value()) == 1);
    }
    SECTION("5") {
        expr.decorateValue(sym.temporary(sym.S16()), fromCat);
        auto conv = computeConversion(ConversionKind::Implicit,
                                      &expr,
                                      sym.U32(),
                                      toCat);
        CHECK(!conv);
    }
}

TEST_CASE("Arithemetic conversions", "[sema]") {
    sema::SymbolTable sym;
    IssueHandler iss;
    sema::AnalysisContext ctx(sym, iss);
    ast::UnaryExpression
        base(ast::UnaryOperator::Promotion,
             ast::UnaryOperatorNotation::Prefix,
             allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                            ast::UnaryOperatorNotation::Prefix,
                                            nullptr,
                                            SourceRange{}),
             SourceRange{});
    auto* expr = base.operand();
    auto setType = [&](QualType type) {
        expr->decorateValue(sym.temporary(type), LValue);
    };
    auto set = [&](QualType type, auto value) {
        setType(type);
        auto* arithType = cast<ArithmeticType const*>(type.get());
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

    DtorStack dtors;

    /// # Widening
    SECTION("u32(5) to u64") {
        set(sym.U32(), 5);
        CHECK(convert(Implicit, expr, sym.U64(), RValue, dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("u64(5) to u64") {
        set(sym.U64(), 5);
        CHECK(convert(Implicit, expr, sym.U64(), RValue, dtors, ctx));
        CHECK(iss.empty());
    }

    /// # Explicit widening
    SECTION("byte(5) to s64") {
        set(sym.Byte(), 5);
        CHECK(convert(Explicit, expr, sym.S64(), RValue, dtors, ctx));
        CHECK(iss.empty());
    }

    /// # Narrowing
    SECTION("s64(5) to s8") {
        set(sym.S64(), 5);
        CHECK(convert(Implicit, expr, sym.S8(), RValue, dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("s64(<unknown>) to s8") {
        setType(sym.S64());
        CHECK(!convert(Implicit, expr, sym.S8(), RValue, dtors, ctx));
        CHECK(convert(Explicit, expr, sym.S8(), RValue, dtors, ctx));
    }
    SECTION("S64(-1) to u32") {
        set(sym.S64(), -1);
        CHECK(!convert(Implicit, expr, sym.U32(), RValue, dtors, ctx));
        CHECK(convert(Explicit, expr, sym.U32(), RValue, dtors, ctx));
        APInt result = getResult();
        CHECK(result.bitwidth() == 32);
        CHECK(ucmp(result, ~0u) == 0);
    }
    SECTION("U32(0x1000000F) to s16") {
        set(sym.U32(), 0x1000000F);
        CHECK(!convert(Implicit, expr, sym.S16(), RValue, dtors, ctx));
        CHECK(convert(Explicit, expr, sym.S16(), RValue, dtors, ctx));
        APInt result = getResult();
        CHECK(result.bitwidth() == 16);
        CHECK(ucmp(result, 0xF) == 0);
    }
    SECTION("-1 to u64") {
        set(sym.S64(), -1);
        CHECK(!convert(Implicit, expr, sym.U64(), RValue, dtors, ctx));
        CHECK(convert(Explicit, expr, sym.U32(), RValue, dtors, ctx));
    }
    SECTION("s64(5) to byte") {
        set(sym.S64(), 5);
        CHECK(convert(Implicit, expr, sym.Byte(), RValue, dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("s64(256) to byte") {
        set(sym.S64(), 256);
        CHECK(!convert(Implicit, expr, sym.Byte(), RValue, dtors, ctx));
    }
    /// No destructors shall have been emitted since we only convert between
    /// trivial types
    CHECK(dtors.empty());
}

TEST_CASE("Common type", "[sema]") {
    SymbolTable sym;
    auto S64 = sym.S64();
    auto Byte = sym.Byte();
    auto U64 = sym.U64();
    auto U32 = sym.U32();

    SECTION("s64, s64 -> s64") {
        CHECK(commonType(sym, S64, S64) == QualType::Mut(S64));
    }
    SECTION("s64, byte -> None") { CHECK(!commonType(sym, S64, Byte)); }
    SECTION("s64, u64 -> None") { CHECK(!commonType(sym, S64, U64)); }
    SECTION("s64, u32 -> s64") {
        CHECK(commonType(sym, S64, U32) == QualType::Mut(S64));
    }
}
