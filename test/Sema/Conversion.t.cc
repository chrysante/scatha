#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>

#include "AST/AST.h"
#include "Common/Allocator.h"
#include "Sema/Analysis/AnalysisContext.h"
#include "Sema/Analysis/ConstantExpressions.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/CleanupStack.h"
#include "Sema/Entity.h"
#include "Sema/SemaUtil.h"
#include "Sema/SimpleAnalzyer.h"
#include "Sema/SymbolTable.h"
#include "Sema/ThinExpr.h"

using namespace scatha;
using namespace sema;
using namespace test;
using enum ValueCategory;
using enum ConversionKind;

TEST_CASE("Implicit conversion rank", "[sema][conv]") {
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

TEST_CASE("Arithemetic conversions", "[sema][conv]") {
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
        return cast<IntValue const*>(base.operand()->constantValue())->value();
    };

    CleanupStack dtors;

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

TEST_CASE("Common type", "[sema][conv]") {
    SymbolTable sym;
    auto* S64 = sym.S64();
    auto* Byte = sym.Byte();
    auto* U64 = sym.U64();
    auto* U32 = sym.U32();

    CHECK(commonType(sym, S64, S64) == QualType::Mut(S64));
    CHECK(!commonType(sym, S64, Byte));
    CHECK(!commonType(sym, S64, U64));
    CHECK(commonType(sym, S64, U32) == QualType::Mut(S64));
    CHECK(commonType(sym,
                     sym.pointer(QualType::Mut(S64)),
                     sym.pointer(QualType::Const(S64)))
              .get() == sym.pointer(QualType::Const(S64)));
}

TEST_CASE("Object construction", "[sema][conv]") {
    auto [ast, sym, iss] = test::produceDecoratedASTAndSymTable(R"(
struct Triv {}
struct NontrivDefault { fn new(&mut this) {} }
struct NoDefault { fn new(&mut this, rhs: &NoDefault) {} }
struct Aggregate { var n: int; var nodef: NoDefault; }
)");
    using enum ConversionKind;
    using enum ObjectTypeConversion;

    auto* Triv = lookup<StructType>(sym, "Triv");
    auto* NontrivDefault = lookup<StructType>(sym, "NontrivDefault");
    auto* NoDefault = lookup<StructType>(sym, "NoDefault");
    auto* Aggregate = lookup<StructType>(sym, "Aggregate");

    /// Trivial type
    CHECK(computeObjectConstruction(Implicit, Triv, {}).value() ==
          TrivDefConstruct);
    CHECK(computeObjectConstruction(Implicit,
                                    Triv,
                                    std::array{ ThinExpr(Triv, LValue) })
              .value() == TrivCopyConstruct);
    CHECK(computeObjectConstruction(Implicit,
                                    Triv,
                                    std::array{ ThinExpr(Triv, RValue) })
              .isNoop());

    /// Trivial type with user defined default constructor
    CHECK(computeObjectConstruction(Implicit, NontrivDefault, {}).value() ==
          NontrivConstruct);

    /// Type without default constructor
    CHECK(computeObjectConstruction(Implicit, NoDefault, {}).isError());

    /// Nontrivial aggregate type
    CHECK(computeObjectConstruction(Explicit,
                                    Aggregate,
                                    std::array{ ThinExpr(sym.Int(), LValue),
                                                ThinExpr(NoDefault, LValue) })
              .value() == NontrivAggrConstruct);

    /// Dynamic array of nontrivial type
    CHECK(computeObjectConstruction(Explicit,
                                    sym.arrayType(Aggregate),
                                    std::array{ ThinExpr(sym.Int(), LValue) })
              .value() == DynArrayConstruct);
}
