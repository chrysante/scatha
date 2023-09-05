#include <Catch/Catch2.hpp>

#include <array>

#include "AST/AST.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Context.h"
#include "Sema/DTorStack.h"
#include "Sema/Entity.h"
#include "Sema/SymbolTable.h"

using namespace scatha;
using namespace sema;

TEST_CASE("Implicit conversion rank", "[sema]") {
    sema::SymbolTable sym;
    ast::UnaryExpression expr(ast::UnaryOperator::Promotion,
                              ast::UnaryOperatorNotation::Prefix,
                              nullptr,
                              SourceRange{});
    SECTION("1") {
        expr.decorateExpr(nullptr, sym.U16());
        auto conv =
            computeConversion(ConversionKind::Implicit, &expr, sym.U16());
        CHECK(computeRank(conv.value()) == 0);
    }
    SECTION("2") {
        expr.decorateExpr(nullptr, sym.S64());
        auto conv =
            computeConversion(ConversionKind::Implicit, &expr, sym.S64());
        CHECK(computeRank(conv.value()) == 0);
    }
    SECTION("3") {
        expr.decorateExpr(nullptr, sym.U16());
        auto conv =
            computeConversion(ConversionKind::Implicit, &expr, sym.S32());
        CHECK(computeRank(conv.value()) == 1);
    }
    SECTION("4") {
        expr.decorateExpr(nullptr, sym.U16());
        auto conv =
            computeConversion(ConversionKind::Implicit, &expr, sym.U32());
        CHECK(computeRank(conv.value()) == 1);
    }
    SECTION("5") {
        expr.decorateExpr(nullptr, sym.S16());
        auto conv =
            computeConversion(ConversionKind::Implicit, &expr, sym.U32());
        CHECK(!conv);
    }
}

TEST_CASE("Arithemetic conversions", "[sema]") {
    sema::SymbolTable sym;
    IssueHandler iss;
    sema::Context ctx(sym, iss);
    ast::UnaryExpression
        base(ast::UnaryOperator::Promotion,
             ast::UnaryOperatorNotation::Prefix,
             allocate<ast::UnaryExpression>(ast::UnaryOperator::Promotion,
                                            ast::UnaryOperatorNotation::Prefix,
                                            nullptr,
                                            SourceRange{}),
             SourceRange{});
    auto* expr = base.operand();
    auto setType = [&](QualType type) { expr->decorateExpr(nullptr, type); };
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

    DTorStack dtors;

    /// # Widening
    SECTION("u32(5) to u64") {
        set(sym.U32(), 5);
        CHECK(convertImplicitly(expr, sym.U64(), dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("u64(5) to u64") {
        set(sym.U64(), 5);
        CHECK(convertImplicitly(expr, sym.U64(), dtors, ctx));
        CHECK(iss.empty());
    }

    /// # Explicit widening
    SECTION("byte(5) to s64") {
        set(sym.Byte(), 5);
        CHECK(convertExplicitly(expr, sym.S64(), dtors, ctx));
        CHECK(iss.empty());
    }

    /// # Narrowing
    SECTION("s64(5) to s8") {
        set(sym.S64(), 5);
        CHECK(convertImplicitly(expr, sym.S8(), dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("s64(<unknown>) to s8") {
        setType(sym.S64());
        CHECK(!convertImplicitly(expr, sym.S8(), dtors, ctx));
        CHECK(convertExplicitly(expr, sym.S8(), dtors, ctx));
    }
    SECTION("S64(-1) to u32") {
        set(sym.S64(), -1);
        CHECK(!convertImplicitly(expr, sym.U32(), dtors, ctx));
        CHECK(convertExplicitly(expr, sym.U32(), dtors, ctx));
        APInt result = getResult();
        CHECK(result.bitwidth() == 32);
        CHECK(ucmp(result, ~0u) == 0);
    }
    SECTION("U32(0x1000000F) to s16") {
        set(sym.U32(), 0x1000000F);
        CHECK(!convertImplicitly(expr, sym.S16(), dtors, ctx));
        CHECK(convertExplicitly(expr, sym.S16(), dtors, ctx));
        APInt result = getResult();
        CHECK(result.bitwidth() == 16);
        CHECK(ucmp(result, 0xF) == 0);
    }
    SECTION("-1 to u64") {
        set(sym.S64(), -1);
        CHECK(!convertImplicitly(expr, sym.U64(), dtors, ctx));
        CHECK(convertExplicitly(expr, sym.U32(), dtors, ctx));
    }
    SECTION("s64(5) to byte") {
        set(sym.S64(), 5);
        CHECK(convertImplicitly(expr, sym.Byte(), dtors, ctx));
        CHECK(iss.empty());
    }
    SECTION("s64(256) to byte") {
        set(sym.S64(), 256);
        CHECK(!convertImplicitly(expr, sym.Byte(), dtors, ctx));
    }
    /// No destructors shall have been emitted since we only convert between
    /// trivial types
    CHECK(dtors.empty());
}

TEST_CASE("Common type", "[sema]") {
    SymbolTable sym;
    auto S64 = sym.S64();
    auto S64MutRef = sym.implRef(QualType::Mut(sym.S64()));
    auto S64ConstRef = sym.implRef(QualType::Const(sym.S64()));
    auto S64MutExplRef = sym.explRef(QualType::Mut(sym.S64()));

    auto S32 = sym.S32();
    auto S32MutRef = sym.implRef(QualType::Mut(sym.S32()));
    auto Byte = sym.Byte();
    auto U64 = sym.U64();
    auto U32 = sym.U32();

    SECTION("s64, s64 -> s64") {
        CHECK(commonType(sym, S64, S64) == QualType::Mut(S64));
    }
    SECTION("'mut s64, 'mut s64 -> 'mut s64") {
        CHECK(commonType(sym, S64MutRef, S64MutRef) ==
              QualType::Mut(S64MutRef));
    }
    SECTION("'s64, 'mut s64 -> 's64") {
        CHECK(commonType(sym, S64ConstRef, S64MutRef) ==
              QualType::Mut(S64ConstRef));
    }
    SECTION("'s64, s64 -> s64") {
        CHECK(commonType(sym, S64ConstRef, S64) == QualType::Mut(S64));
    }
    SECTION("'s64, 'mut s32 -> s64") {
        CHECK(commonType(sym, S64ConstRef, S32MutRef) == QualType::Mut(S64));
    }
    SECTION("'s64, s32 -> s64") {
        CHECK(commonType(sym, S64ConstRef, S32) == QualType::Mut(S64));
    }
    SECTION("&mut s64, 'mut s64 -> None") {
        CHECK(!commonType(sym, S64MutExplRef, S64MutRef));
    }
    SECTION("s64, byte -> None") { CHECK(!commonType(sym, S64, Byte)); }
    SECTION("s64, u64 -> None") { CHECK(!commonType(sym, S64, U64)); }
    SECTION("s64, u32 -> s64") {
        CHECK(commonType(sym, S64, U32) == QualType::Mut(S64));
    }
}
